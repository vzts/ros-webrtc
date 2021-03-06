#ifndef ROS_WEBRTC_RENDERER_H_
#define ROS_WEBRTC_RENDERER_H_

#include <string>

#include <boost/shared_ptr.hpp>
#include <ros/ros.h>
#include <ros_webrtc/Audio.h>
#include <ros_webrtc/Data.h>
#include <sensor_msgs/Image.h>
#include <webrtc/api/mediastreaminterface.h>
#include <webrtc/api/datachannelinterface.h>
#include <webrtc/base/scoped_ref_ptr.h>
#include <webrtc/media/base/videosinkinterface.h>

class AudioSink : public webrtc::AudioTrackSinkInterface {

public:
    AudioSink(
        ros::NodeHandle& nh,
        const std::string& topic,
        uint32_t queue_size,
        webrtc::AudioTrackInterface* audio_track
    );

    ~AudioSink();

    webrtc::AudioTrackInterface* audio_track();

private:
    rtc::scoped_refptr<webrtc::AudioTrackInterface> _audio_track;

    ros_webrtc::Audio _msg;

    ros::Publisher _rpub;

// webrtc::AudioTrackSinkInterface

public:

    virtual void OnData(
        const void* audio_data,
        int bits_per_sample,
        int sample_rate,
        size_t number_of_channels,
        size_t number_of_frames
    );

};

typedef boost::shared_ptr<AudioSink> AudioSinkPtr;

class VideoRenderer : public rtc::VideoSinkInterface<cricket::VideoFrame> {

public:

    VideoRenderer(
        ros::NodeHandle nh,
        const std::string& topic,
        uint32_t queue_size,
        webrtc::VideoTrackInterface* video_track
    );

    ~VideoRenderer();

    webrtc::VideoTrackInterface* video_track();

    void Close();

private:

    rtc::scoped_refptr<webrtc::VideoTrackInterface> _video_track;

    ros::Publisher _rpub;

    sensor_msgs::Image _msg;

// rtc::VideoSinkInterface<cricket::VideoFrame>

public:

    virtual void OnFrame(const cricket::VideoFrame& frame);

};

typedef boost::shared_ptr<VideoRenderer> VideoRendererPtr;

class DataObserver : public webrtc::DataChannelObserver {

public:

    DataObserver(
        ros::NodeHandle& nh,
        const std::string& topic,
        uint32_t queue_size,
        webrtc::DataChannelInterface* data_channel
    );

    virtual ~DataObserver();

    virtual size_t reap() = 0;

protected:

    rtc::scoped_refptr<webrtc::DataChannelInterface> _dc;

    ros::Publisher _rpub;

// webrtc::DataChannelObserver

public:

    virtual void OnStateChange();

};

typedef boost::shared_ptr<DataObserver> DataObserverPtr;

typedef boost::shared_ptr<const DataObserver> DataObserverConstPtr;


class UnchunkedDataObserver : public DataObserver {

public:

    UnchunkedDataObserver(
        ros::NodeHandle& nh,
        const std::string& topic,
        uint32_t queue_size,
        webrtc::DataChannelInterface* data_channel
    );

// DataObserver

public:

    virtual size_t reap();

// webrtc::DataChannelObserver

public:

    virtual void OnMessage(const webrtc::DataBuffer& buffer);

};

class ChunkedDataObserver : public DataObserver {

public:

    ChunkedDataObserver(
        ros::NodeHandle& nh,
        const std::string& topic,
        uint32_t queue_size,
        webrtc::DataChannelInterface* data_channel
    );

private:

    struct Message {

        Message(
            const std::string& id,
            size_t count,
            const ros::Duration& duration
        );

        void add_chunk(size_t index, const std::string& data);

        bool is_complete() const;

        bool is_expired() const;

        void merge(ros_webrtc::Data& msg);

        std::string id;

        size_t count;

        ros::Time expires_at;

        struct Chunk {

            Chunk(size_t index, const std::string &buffer);

            size_t index;

            std::vector<uint8_t> buffer;

        };

        std::list<Chunk> chunks;

    };

    typedef boost::shared_ptr<Message> MessagePtr;

    typedef std::map<std::string, MessagePtr> Messages;

    Messages _messages;


// DataObserver

public:

    virtual size_t reap();

// webrtc::DataChannelObserver

public:

    virtual void OnMessage(const webrtc::DataBuffer& buffer);

};

#endif /* ROS_WEBRTC_RENDERER_H_ */
