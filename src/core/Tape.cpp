#include "Tape.h"

namespace keytape
{
// ======================================================================== tracks and reels

int TapeTrack::takeAt (int64_t p) const
{
    for (int i = 0; i < (int) takes.size(); ++i)
        if (p >= takes[(size_t) i].start && p < takes[(size_t) i].end())
            return i;
    return -1;
}

float TapeTrack::read (double p, int ch, int& hint) const
{
    // the hint remembers the last take, which is nearly always the next one asked for
    auto in = [&] (int i) { return i >= 0 && i < (int) takes.size() && p >= (double) takes[(size_t) i].start && p < (double) takes[(size_t) i].end(); };
    if (! in (hint))
    {
        hint = -1;
        for (int i = 0; i < (int) takes.size(); ++i)
            if (in (i))
            {
                hint = i;
                break;
            }
        if (hint < 0)
            return 0.0f;
    }
    const auto& t = takes[(size_t) hint];
    if (t.audio == nullptr)
        return 0.0f;
    const double local = p - (double) t.start + (double) t.offset;
    const int i0 = (int) local;
    const int len = t.audio->length();
    if (i0 < 0 || i0 >= len)
        return 0.0f;
    const float* d = t.audio->data.getReadPointer (std::min (ch, t.audio->data.getNumChannels() - 1));
    const float f = (float) (local - i0);
    return d[i0] + ((i0 + 1 < len ? d[i0 + 1] : d[i0]) - d[i0]) * f;
}

bool Reel::empty() const
{
    for (auto& t : tracks)
        if (! t.takes.empty())
            return false;
    return true;
}

int64_t Reel::lastEnd() const
{
    int64_t e = 0;
    for (auto& t : tracks)
        for (auto& k : t.takes)
            e = std::max (e, k.end());
    return e;
}

// ======================================================================== the machine

void TapeMachine::prepare (double sampleRate, int)
{
    sr = sampleRate;
    // (the recording buffer, six minutes of stereo, is only made when something is first recorded)
    if (recBuf.getNumSamples() > 0)
        recBuf.setSize (2, (int) tapeLength() + 4, false, true);
}

void TapeMachine::ensureRecBuffer()
{
    if (recBuf.getNumSamples() < (int) tapeLength() + 4)
        recBuf.setSize (2, (int) tapeLength() + 4, false, true);
}

void TapeMachine::play()
{
    playing = true;
    breaking = false;
}

void TapeMachine::stop()
{
    if (recording || armed)
        finishRecording();
    playing = false;
    recording = armed = false;
    shuttleDir = 0;
}

void TapeMachine::record()
{
    if (recording)
        return;
    ensureRecBuffer();
    recording = true;
    armed = false;
    playing = true;
    recMin = recMax = -1;
    lastRecPos = -1.0;
}

void TapeMachine::arm()
{
    if (recording || armed)
    {
        stop();
        return;
    }
    ensureRecBuffer();
    armed = true;
    recording = false;
    playing = false;
    recMin = recMax = -1;
    lastRecPos = -1.0;
}

void TapeMachine::locate (int64_t p)
{
    pos = (double) juce::jlimit<int64_t> (0, tapeLength() - 1, p);
    virtualPos = pos;
    lastRecPos = -1.0;
}

void TapeMachine::shuttle (int direction) { shuttleDir = direction; }

void TapeMachine::scrub (int ticks)
{
    // a tick moves the tape about a tenth of a second (a turn of the encoder is a couple of seconds)
    scrubVelocity += (float) ticks * 0.1f * (float) sr / 512.0f;
}

void TapeMachine::toggleLoop()
{
    if (reel.loopIn >= 0 && reel.loopOut > reel.loopIn)
        reel.loopOn = ! reel.loopOn;
    else
        reel.loopOn = false;
    reel.dirty = true;
}

void TapeMachine::loopCurrentTake()
{
    if (const auto* t = activeTake())
    {
        reel.loopIn = t->start;
        reel.loopOut = t->end();
        reel.loopOn = true;
        reel.dirty = true;
    }
}

void TapeMachine::setBreak (bool on)
{
    if (on && ! breaking)
    {
        breaking = true;
        breakSpeed = 1.0f;
        virtualPos = pos;
    }
    else if (! on && breaking)
    {
        breaking = false;
        pos = virtualPos; // (the time ran on in the background)
    }
}

void TapeMachine::setChop (bool on, double bpm)
{
    if (on && ! chopping)
    {
        chopping = true;
        chopStart = pos;
        chopLen = 60.0 / std::max (20.0, bpm) / 4.0 * sr * std::abs (speed); // a sixteenth of the tape at its speed
        virtualPos = pos;
    }
    else if (! on && chopping)
    {
        chopping = false;
        pos = virtualPos;
    }
}

void TapeMachine::moveLoopByBar (int dir)
{
    const int64_t bar = barLength();
    if (reel.loopIn < 0 || reel.loopOut <= reel.loopIn)
        return;
    const int64_t len = reel.loopOut - reel.loopIn;
    reel.loopIn = juce::jlimit<int64_t> (0, tapeLength() - len, reel.loopIn + dir * bar);
    reel.loopOut = reel.loopIn + len;
    reel.dirty = true;
}

void TapeMachine::jumpBar (int dir)
{
    const int64_t bar = barLength();
    const int64_t cur = (int64_t) pos;
    int64_t target = dir > 0 ? (cur / bar + 1) * bar : ((cur - 1) / bar) * bar;
    if (dir < 0 && cur % bar != 0)
        target = (cur / bar) * bar;
    locate (target);
}

int64_t TapeMachine::snap (int64_t p) const
{
    if (gridResolution <= 0)
        return p;
    const int64_t step = barLength() / (gridResolution == 1 ? 1 : gridResolution == 2 ? 4 : 8);
    return step > 0 ? ((p + step / 2) / step) * step : p;
}

void TapeMachine::fixLoop()
{
    if (reel.loopIn >= 0 && reel.loopOut >= 0 && reel.loopOut < reel.loopIn)
        std::swap (reel.loopIn, reel.loopOut);
    if (reel.loopIn < 0 || reel.loopOut <= reel.loopIn)
        reel.loopOn = false;
    reel.dirty = true;
}

// ======================================================================== audio

void TapeMachine::writeRec (double p, float l, float r)
{
    // written at the tape's own rate: between the last write and this one every tape sample gets a value (a fast
    // tape stretches the sound out); slower than normal, several writes land on one sample and the last wins
    const int64_t i = (int64_t) std::llround (p);
    if (i < 0 || i >= tapeLength())
        return;
    auto put = [this] (int64_t k, float a, float b) {
        recBuf.setSample (0, (int) k, a);
        recBuf.setSample (1, (int) k, b);
        recMin = recMin < 0 ? k : std::min (recMin, k);
        recMax = std::max (recMax, k);
    };
    if (lastRecPos >= 0.0)
    {
        const int64_t j = (int64_t) std::llround (lastRecPos);
        const int64_t gap = std::abs (i - j);
        if (gap > 1 && gap < 4096)
            for (int64_t k = 1; k < gap; ++k)
            {
                const float f = (float) k / (float) gap;
                put (j + (i > j ? k : -k), lastRecL + (l - lastRecL) * f, lastRecR + (r - lastRecR) * f);
            }
    }
    put (i, l, r);
    lastRecPos = p;
    lastRecL = l;
    lastRecR = r;
}

void TapeMachine::process (int n, const float* inL, const float* inR, std::array<std::array<float*, 2>, kTracks>& out)
{
    for (auto& t : out)
        for (auto* c : t)
            if (c != nullptr)
                std::fill (c, c + n, 0.0f);
    const double length = (double) tapeLength();
    const bool loop = reel.loopOn && reel.loopIn >= 0 && reel.loopOut > reel.loopIn;
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        // how fast and which way the tape moves this sample
        double v = 0.0;
        if (playing)
            v = speed * ((reversePlay != trickReverse) ? -1.0 : 1.0);
        if (shuttleDir != 0)
            v = 8.0 * shuttleDir;
        if (scrubVelocity != 0.0f)
        {
            v += scrubVelocity;
            scrubVelocity *= 0.9993f;
            if (std::abs (scrubVelocity) < 0.01f)
                scrubVelocity = 0.0f;
        }
        if (breaking)
        {
            virtualPos += v;
            breakSpeed *= 0.9998f;
            v *= breakSpeed;
        }
        if (chopping)
            virtualPos += v;
        // read the four tracks at the head
        // (while recording, what this pass already wrote is what's on the selected track: looped passes build up)
        const int64_t at = (int64_t) std::llround (pos);
        const bool fromRec = (recording || armed) && recMin >= 0 && at >= recMin && at <= recMax;
        for (int t = 0; t < kTracks; ++t)
        {
            const bool rec = fromRec && t == track;
            const float l = rec ? recBuf.getSample (0, (int) at) : reel.tracks[(size_t) t].read (pos, 0, hints[(size_t) t]);
            const float r = rec ? recBuf.getSample (1, (int) at) : reel.tracks[(size_t) t].read (pos, 1, hints[(size_t) t]);
            if (out[(size_t) t][0] != nullptr)
                out[(size_t) t][0][i] = l;
            if (out[(size_t) t][1] != nullptr)
                out[(size_t) t][1][i] = r;
        }
        // record onto the selected track: what's there plus what's played (overdub)
        if ((recording || armed) && v != 0.0)
        {
            const float l = (out[(size_t) track][0] != nullptr ? out[(size_t) track][0][i] : 0.0f) + (inL != nullptr ? inL[i] * recLevel : 0.0f);
            const float r = (out[(size_t) track][1] != nullptr ? out[(size_t) track][1][i] : 0.0f) + (inR != nullptr ? inR[i] * recLevel : 0.0f);
            writeRec (pos, l, r);
            peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
        }
        pos += v;
        if (chopping && pos >= chopStart + chopLen)
            pos = chopStart;
        if (loop && v > 0.0 && pos >= (double) reel.loopOut)
            pos = (double) reel.loopIn + (pos - (double) reel.loopOut);
        if (loop && v < 0.0 && pos < (double) reel.loopIn)
            pos = (double) reel.loopOut - ((double) reel.loopIn - pos);
        if (pos < 0.0)
        {
            pos = 0.0;
            if (playing && v < 0.0)
                playing = false;
        }
        if (pos >= length - 1.0)
        {
            pos = length - 1.0;
            if (recording)
            {
                recording = false;
                pendingTake = true;
            }
            playing = false;
        }
    }
    lastLevel = std::max (peak, lastLevel * 0.9f);
}

bool TapeMachine::takePendingTake()
{
    if (! pendingTake)
        return false;
    pendingTake = false;
    finishRecording();
    return true;
}

void TapeMachine::finishRecording()
{
    if (recMin < 0 || recMax <= recMin)
    {
        recMin = recMax = -1;
        return;
    }
    // the new take: what was recorded between the lowest and highest positions written
    const int len = (int) (recMax - recMin + 1);
    auto a = std::make_shared<Audio>();
    a->sampleRate = sr;
    a->data.setSize (2, len);
    for (int ch = 0; ch < 2; ++ch)
        a->data.copyFrom (ch, 0, recBuf, ch, (int) recMin, len);
    Take take;
    take.audio = a;
    take.start = recMin;
    take.length = len;
    insertTake (track, take);
    recMin = recMax = -1;
    lastRecPos = -1.0;
}

void TapeMachine::removeRange (int t, int64_t from, int64_t to)
{
    // cut [from, to) out of a track: takes inside go, takes across the edges are trimmed or split
    auto& takes = reel.tracks[(size_t) t].takes;
    std::vector<Take> kept;
    for (auto& k : takes)
    {
        if (k.end() <= from || k.start >= to)
        {
            kept.push_back (k);
            continue;
        }
        if (k.start < from)
        {
            Take left = k;
            left.length = from - k.start;
            kept.push_back (left);
        }
        if (k.end() > to)
        {
            Take right = k;
            right.offset = k.offset + (to - k.start);
            right.start = to;
            right.length = k.end() - to;
            kept.push_back (right);
        }
    }
    takes = kept;
}

void TapeMachine::insertTake (int t, Take take)
{
    removeRange (t, take.start, take.end());
    auto& takes = reel.tracks[(size_t) t].takes;
    takes.push_back (take);
    std::sort (takes.begin(), takes.end(), [] (const Take& a, const Take& b) { return a.start < b.start; });
    reel.dirty = true;
}

AudioPtr TapeMachine::render (int t, int64_t from, int64_t to) const
{
    auto a = std::make_shared<Audio>();
    a->sampleRate = sr;
    const int len = (int) std::max<int64_t> (1, to - from);
    a->data.setSize (2, len);
    a->data.clear();
    int hint = -1;
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = a->data.getWritePointer (ch);
        for (int i = 0; i < len; ++i)
            d[i] = reel.tracks[(size_t) t].read ((double) (from + i), ch, hint);
    }
    return a;
}

const Take* TapeMachine::activeTake() const
{
    const auto& tr = reel.tracks[(size_t) track];
    const int i = tr.takeAt ((int64_t) pos);
    return i >= 0 ? &tr.takes[(size_t) i] : nullptr;
}

bool TapeMachine::lift()
{
    clipboardAll = false;
    for (auto& c : clipboard)
        c.reset();
    if (reel.loopIn >= 0 && reel.loopOut > reel.loopIn)
    {
        // region lift: the loop's stretch of the track
        Take take;
        take.audio = render (track, reel.loopIn, reel.loopOut);
        take.start = reel.loopIn;
        take.length = reel.loopOut - reel.loopIn;
        clipboard[(size_t) track] = take;
        removeRange (track, reel.loopIn, reel.loopOut);
        reel.dirty = true;
        return true;
    }
    const auto& tr = reel.tracks[(size_t) track];
    const int i = tr.takeAt ((int64_t) pos);
    if (i < 0)
        return false;
    clipboard[(size_t) track] = tr.takes[(size_t) i];
    reel.tracks[(size_t) track].takes.erase (reel.tracks[(size_t) track].takes.begin() + i);
    reel.dirty = true;
    return true;
}

bool TapeMachine::liftAll()
{
    clipboardAll = true;
    for (auto& c : clipboard)
        c.reset();
    // the loop's region if set, or the stretch the takes under the head cover
    int64_t from = reel.loopIn, to = reel.loopOut;
    if (from < 0 || to <= from)
    {
        from = std::numeric_limits<int64_t>::max();
        to = -1;
        for (auto& tr : reel.tracks)
            if (const int i = tr.takeAt ((int64_t) pos); i >= 0)
            {
                from = std::min (from, tr.takes[(size_t) i].start);
                to = std::max (to, tr.takes[(size_t) i].end());
            }
        if (to <= from)
            return false;
    }
    for (int t = 0; t < kTracks; ++t)
    {
        Take take;
        take.audio = render (t, from, to);
        take.start = from;
        take.length = to - from;
        clipboard[(size_t) t] = take;
        removeRange (t, from, to);
    }
    reel.dirty = true;
    return true;
}

AudioPtr TapeMachine::clipboardMix() const
{
    int64_t len = 0;
    for (auto& c : clipboard)
        if (c)
            len = std::max (len, c->length);
    if (len <= 0)
        return {};
    auto a = std::make_shared<Audio>();
    a->sampleRate = sr;
    a->data.setSize (2, (int) len);
    a->data.clear();
    for (auto& c : clipboard)
        if (c && c->audio != nullptr)
            for (int ch = 0; ch < 2; ++ch)
                a->data.addFrom (ch, 0, c->audio->data, std::min (ch, c->audio->data.getNumChannels() - 1), (int) c->offset,
                                 (int) std::min<int64_t> (c->length, c->audio->length() - c->offset));
    return a;
}

bool TapeMachine::drop()
{
    bool any = false;
    int64_t end = (int64_t) pos;
    for (int t = 0; t < kTracks; ++t)
    {
        const auto& c = clipboard[(size_t) t];
        if (! c)
            continue;
        Take take = *c;
        take.start = (int64_t) pos;
        if (take.end() > tapeLength())
            take.length = tapeLength() - take.start;
        if (take.length <= 0)
            continue;
        insertTake (clipboardAll ? t : track, take);
        end = std::max (end, take.end());
        any = true;
    }
    if (any)
        locate (end); // (repeated drops lay the takes end to end)
    return any;
}

bool TapeMachine::split()
{
    auto& takes = reel.tracks[(size_t) track].takes;
    const int64_t p = (int64_t) pos;
    const int i = reel.tracks[(size_t) track].takeAt (p);
    if (i < 0 || p <= takes[(size_t) i].start)
        return false;
    Take right = takes[(size_t) i];
    right.offset += p - right.start;
    right.length = right.end() - p;
    right.start = p;
    takes[(size_t) i].length = p - takes[(size_t) i].start;
    takes.insert (takes.begin() + i + 1, right);
    reel.dirty = true;
    return true;
}

bool TapeMachine::join()
{
    // the active take joins the nearest take on either side (the gap between becomes silence)
    auto& takes = reel.tracks[(size_t) track].takes;
    const int i = reel.tracks[(size_t) track].takeAt ((int64_t) pos);
    if (i < 0 || takes.size() < 2)
        return false;
    int j = -1;
    int64_t best = std::numeric_limits<int64_t>::max();
    if (i > 0 && takes[(size_t) i].start - takes[(size_t) i - 1].end() < best)
    {
        best = takes[(size_t) i].start - takes[(size_t) i - 1].end();
        j = i - 1;
    }
    if (i + 1 < (int) takes.size() && takes[(size_t) i + 1].start - takes[(size_t) i].end() < best)
        j = i + 1;
    if (j < 0)
        return false;
    const int a = std::min (i, j), b = std::max (i, j);
    const int64_t from = takes[(size_t) a].start, to = takes[(size_t) b].end();
    Take joined;
    joined.audio = render (track, from, to);
    joined.start = from;
    joined.length = to - from;
    takes.erase (takes.begin() + b);
    takes[(size_t) a] = joined;
    reel.dirty = true;
    return true;
}

void TapeMachine::slide (int ticks)
{
    auto& takes = reel.tracks[(size_t) track].takes;
    const int i = reel.tracks[(size_t) track].takeAt ((int64_t) pos);
    if (i < 0)
        return;
    auto& t = takes[(size_t) i];
    const int64_t step = (int64_t) (sr * 0.02) * ticks;
    const int64_t lo = i > 0 ? takes[(size_t) i - 1].end() : 0;
    const int64_t hi = i + 1 < (int) takes.size() ? takes[(size_t) i + 1].start : tapeLength();
    const int64_t newStart = juce::jlimit (lo, std::max (lo, hi - t.length), t.start + step);
    const int64_t moved = newStart - t.start;
    t.start = newStart;
    locate ((int64_t) pos + moved); // (the take stays under the head)
    reel.dirty = true;
}

void TapeMachine::eraseAll()
{
    stop();
    for (auto& t : reel.tracks)
        t.takes.clear();
    reel.loopIn = reel.loopOut = -1;
    reel.loopOn = false;
    locate (0);
    reel.dirty = true;
}

void TapeMachine::dropSoundData (const juce::var& sound, int64_t lengthSamples)
{
    // a sound saved to tape: a short silent take carrying the sound's data
    auto a = std::make_shared<Audio>();
    a->sampleRate = sr;
    a->data.setSize (2, (int) std::max<int64_t> (1, lengthSamples));
    a->data.clear();
    Take take;
    take.audio = a;
    take.start = (int64_t) pos;
    take.length = a->length();
    take.soundData = sound;
    insertTake (track, take);
    locate (take.end());
}
} // namespace keytape
