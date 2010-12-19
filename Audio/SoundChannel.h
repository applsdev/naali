// For conditions of distribution and use, see copyright notice in license.txt
#ifndef incl_OpenALAudio_SoundChannel_h
#define incl_OpenALAudio_SoundChannel_h

#include "SoundBuffer.h"
#include "AssetFwd.h"
#include "AudioApiExports.h"
#include "Vector3D.h"
#include "AudioFwd.h"
#include "AudioAsset.h"

/// An OpenAL sound channel (source).
class AUDIO_API SoundChannel : public QObject
{
    Q_OBJECT;
    Q_ENUMS(SoundState)
    Q_ENUMS(SoundType)

public:
    /// States of sound channels
    enum SoundState
    {
        Stopped = 0,
        Pending,
        Playing
    };
   
    /// Types of sound channels, for adjusting master volume individually
    enum SoundType
    {
        Triggered = 0,
        Ambient,
        Voice
    };

    SoundChannel(sound_id_t channelId, SoundType type);

    ~SoundChannel();
    
    /// Start playing sound. Set to pending state if sound is actually not loaded yet
    void Play(AudioAssetPtr audioAsset);

    /// Add a sound buffer and play.
    /** Note: after a sound buffer is added, channel will remain in pending state even if there is no 
        more sound data for the moment. This is to ensure that the sound system will not automatically
        dispose of the channel. */ 
    void AddBuffer(const SoundBuffer &buffer);
    
    /// Set positional state
    void SetPositional(bool enable);
    /// Set looped state
    void SetLooped(bool enable);
    /// Set position
    void SetPosition(const Vector3df& pos);
    /// Set pitch.
    void SetPitch(float pitch);
    /// Set gain.
    void SetGain(float gain);
    /// Set master gain.
    void SetMasterGain(float master_gain);
    /// Set range parameters
    void SetRange(float inner_radius, float outer_radius, float rolloff);
    /// Stop.
    void Stop();
    /// Per-frame update with new listener position
    void Update(const Vector3df& listener_pos);
    /// Return current state of channel.
    SoundState GetState() const { return state_; }
    /// Return name/id of sound that's playing, empty if nothing playing
    QString GetSoundName() const;
    /// Return sound type
    SoundType GetSoundType() const { return type_; }
    /// Return sound gain.
    float GetGain() const {return gain_;}
    /// Return sound pitch.
    float GetPitch() const {return pitch_;}

    sound_id_t GetChannelId() const { return channelId; }
private:
    /// Queue buffers and start playing
    void QueueBuffers();
    /// Remove processed buffers
    void UnqueueBuffers();
    /// Create OpenAL source if one does not exist yet
    bool CreateSource();
    /// Delete OpenAL source
    void DeleteSource();
    /// Calculate attenuation from position, listener position & range parameters
    void CalculateAttenuation(const Vector3df &listener_pos);
    /// Set positionality & position
    void SetPositionAndMode();
    /// Set gain, taking attenuation into account
    void SetAttenuatedGain();
    
    /// Sound type
    SoundType type_;
    /// OpenAL handle
    ALuint handle_;
    /// Sounds buffers pending to be played
    std::list<AudioAssetWeakPtr> pending_sounds_;
    /// Currently playing sound buffers
    std::vector<AudioAssetPtr> playing_sounds_;
    /// Pitch
    float pitch_;
    /// Gain
    float gain_;
    /// Master gain. Final sound volume = gain * master gain * possible distance attenuation
    float master_gain_;
    /// Inner radius
    float inner_radius_;
    /// Outer radius
    float outer_radius_;
    /// Rolloff power factor
    float rolloff_;
    /// Last calculated attenuation factor
    float attenuation_;
    /// Looped flag
    bool looped_;
    /// Positional flag
    bool positional_;
    /// Buffered operation flag. Will never report as stopped, unless explicitly stopped
    bool buffered_mode_;
    /// Position
    Vector3df position_;
    /// State 
    SoundState state_;
    /// Specifies an unique ID for this sound channel. Note that this ID should not be treated as a "channel index" or anything like that.
    sound_id_t channelId;
};

typedef boost::shared_ptr<SoundChannel> SoundChannelPtr;

#endif