#ifndef ROS_WEBRTC_SESSION_H_
#define ROS_WEBRTC_SESSION_H_

#include <list>

#include <boost/shared_ptr.hpp>
#include <bondcpp/bond.h>
#include <ros/ros.h>
#include <ros_webrtc/DataChannel.h>
#include <ros_webrtc/Session.h>
#include <talk/app/webrtc/mediaconstraintsinterface.h>
#include <talk/app/webrtc/peerconnectioninterface.h>
#include <talk/app/webrtc/videosourceinterface.h>
#include <webrtc/base/scoped_ptr.h>
#include <webrtc/base/refcount.h>
#include <webrtc/base/scoped_ref_ptr.h>

#include "media_constraints.h"
#include "media_type.h"
#include "renderer.h"

class Device;

/**
 * \brief Categorized sizes of publisher/subscriber queues.
 */
struct QueueSizes {

    QueueSizes(uint32_t size = 0);

    QueueSizes(uint32_t video, uint32_t audio, uint32_t data, uint32_t event);

    uint32_t video;
    uint32_t audio;
    uint32_t data;
    uint32_t event;

};

/**
 * \brief Represents a peer connection.
 */
class Session {

public:

    /**
     * \brief Creates an initial session.
     * \param id String identifying the session.
     * \param peer_id String identifying the remote peer.
     * \param data_channels Data channel settings (e.g. label, reliability, ordering, etc).
     * \param sdp_constraints Media constraints to apply for this session.
     * \param service_names Names of ROS services to use for signaling (e.g. ice-candidates, sdp-offer-answers, etc).
     * \param default_queue_size Default size of publisher and subscriber queues.
     */
    Session(
        const std::string& id,
        const std::string& peer_id,
        const std::vector<ros_webrtc::DataChannel>& dcs,
        const MediaConstraints& sdp_constraints,
        const std::map<std::string, std::string>& service_names,
        const QueueSizes& queue_sizes
    );

    /**
     * \brief String identifying this session.
     * \return Session identifier.
     */
    const std::string& id() const;

    /**
     * \brief String identifying the remote peer for this session.
     * \return Peer identifier.
     */
    const std::string& peer_id() const;

    /**
     * \brief Video source for a session to be added to local stream as a track.
     */
    struct VideoSource {

        VideoSource(
            const std::string& label,
            webrtc::VideoSourceInterface *interface,
            bool publish = false
        );

        std::string label;

        bool publish;

        rtc::scoped_refptr<webrtc::VideoSourceInterface> interface;

    };

    /**
     * \brief Audio source for a session to be added to local stream as a track.
     */
    struct AudioSource {

        AudioSource(
            const std::string& label,
            webrtc::AudioSourceInterface *interface,
            bool publish = false
        );

        std::string label;

        rtc::scoped_refptr<webrtc::AudioSourceInterface> interface;

        bool publish;

    };

    /**
     * \brief Callback interface used to observe session life-cycle.
     */
    class Observer {

    public:

        virtual ~Observer() {}

        virtual void on_connection_change(webrtc::PeerConnectionInterface::IceConnectionState state) = 0;

    };

    typedef boost::shared_ptr<Observer> ObserverPtr;

    /**
     * \brief Initiate connection to remote peer for this session.
     * \param pc_factory Factory used to create a peer connection.
     * \param pc_constraints Constraints to apply to the connection.
     * \param ice_servers Collection of servers to use for ICE (connection negotiation).
     * \param audio_srcs Local audio sources to share with peer as a stream tracks.
     * \param video_srcs Local video sources to share with peer as a stream tracks.
     * \param observer Optional observer to call on various session life-cycle events (e.g. disconnect).
     * \return Whether initiation of peer connection succeeded.
     */
    bool begin(
        webrtc::PeerConnectionFactoryInterface* pc_factory,
        const webrtc::MediaConstraintsInterface* pc_constraints,
        const webrtc::PeerConnectionInterface::IceServers& ice_servers,
        const std::vector<AudioSource> &audio_srcs,
        const std::vector<VideoSource> &video_srcs,
        ObserverPtr observer = ObserverPtr()
    );

    /**
     * \brief Teardown connection to remote peer, ending this session.
     */
    void end();

    webrtc::PeerConnectionInterface* peer_connection();

    bool create_offer();

    bool is_offerer() const;

    void create_answer();

    void add_remote_ice_candidate(webrtc::IceCandidateInterface* candidate);

    void set_remote_session_description(webrtc::SessionDescriptionInterface* sdp);

    operator ros_webrtc::Session () const;

    struct DataChannel {

        DataChannel(const ros_webrtc::DataChannel& conf);

        void send(const ros_webrtc::DataConstPtr& msg);

        void send(webrtc::DataBuffer& data_buffer);

        bool is_chunked() const;

        size_t chunk_size() const;

        std::string send_topic(const Session& session) const;

        std::string recv_topic(const Session& session) const;

        operator ros_webrtc::DataChannel () const;

        ros_webrtc::DataChannel conf;

        MediaType protocol;

        rtc::scoped_refptr<webrtc::DataChannelInterface> provider;

        typedef std::list<boost::shared_ptr<DataObserver>> Observers;

        Observers observers;

        ros::Subscriber subscriber;

    };

    DataChannel* data_channel(const std::string& label);

    const DataChannel* data_channel(const std::string& label) const;

    struct FlushStats {

        size_t reaped_data_messages;

    };

    FlushStats flush();

private:

    struct ChunkedDataTransfer {

        ChunkedDataTransfer(
            const std::string& id,
            const webrtc::DataBuffer& data_buffer,
            size_t size
        );

        bool is_complete() const;

        size_t operator()(webrtc::DataChannelInterface* provider);

        const std::string id;

        const rtc::Buffer& data;

        const size_t size;

        const size_t total;

        size_t current;

    };

    class PeerConnectionObserver : public webrtc::PeerConnectionObserver {

    public:

        PeerConnectionObserver(Session &instance_);

        Session& instance;

    // webrtc::PeerConnectionObserver

    public:
        virtual void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState new_state);

        virtual void OnStateChange(StateType state_changed);

        virtual void OnAddStream(webrtc::MediaStreamInterface* stream);

        virtual void OnRemoveStream(webrtc::MediaStreamInterface* stream);

        virtual void OnDataChannel(webrtc::DataChannelInterface* data_channel);

        virtual void OnRenegotiationNeeded();

        virtual void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState new_state);

        virtual void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState new_state);

        virtual void OnIceCandidate(const webrtc::IceCandidateInterface* candidate);

        virtual void OnIceComplete();
    };

    class CreateSessionDescriptionObserver :
        public rtc::RefCountedObject<webrtc::CreateSessionDescriptionObserver> {

    public:

        CreateSessionDescriptionObserver(Session &instance_);

        virtual ~CreateSessionDescriptionObserver();

        Session& instance;

    // webrtc::CreateSessionDescriptionObserver

    public:

        virtual void OnSuccess(webrtc::SessionDescriptionInterface* desc);

        virtual void OnFailure(const std::string& error);

    };

    class SetSessionDescriptionObserver :
            public rtc::RefCountedObject<webrtc::SetSessionDescriptionObserver> {

    public:

        SetSessionDescriptionObserver(Session &instance_);

        virtual ~SetSessionDescriptionObserver();

        Session& instance;

    // webrtc::SetSessionDescriptionObserver

    public:

        virtual void OnSuccess();

        virtual void OnFailure(const std::string& error);

    };

    typedef boost::shared_ptr<webrtc::IceCandidateInterface> IceCandidatePtr;

    class ServiceClient {

    public:

        ServiceClient(Session &instance, const std::map<std::string, std::string>& names);

        void shutdown();

        ros::ServiceClient connect_session;

        ros::ServiceClient end_session;

        ros::ServiceClient add_session_ice_candidate;

        ros::ServiceClient set_session_description;

    private:

        Session &_instance;

    };

    bool _open_peer_connection(
        webrtc::PeerConnectionFactoryInterface* pc_factory,
        const webrtc::MediaConstraintsInterface* pc_constraints,
        const webrtc::PeerConnectionInterface::IceServers& ice_servers
    );

    void _close_peer_connection();

    bool _open_local_stream(
        webrtc::PeerConnectionFactoryInterface* pc_factory,
        const std::vector<Session::AudioSource> &audio_srcs,
        const std::vector<Session::VideoSource> &video_srcs
    );

    void _close_local_stream();

    void _on_bond_formed();

    void _on_bond_broken();

    void _on_local_description(webrtc::SessionDescriptionInterface* desc);

    void _drain_remote_ice_candidates();

    void _begin_event();

    void _renegotiation_needed_event();

    void _signaling_state_change_event(webrtc::PeerConnectionInterface::SignalingState new_state);

    void _ice_state_change_event(webrtc::PeerConnectionInterface::IceState new_state);

    void _ice_gathering_change_event(webrtc::PeerConnectionInterface::IceGatheringState new_state);

    void _ice_connection_change_event(webrtc::PeerConnectionInterface::IceConnectionState new_state);

    void _ice_complete_event();

    void _add_stream_event();

    void _remove_stream_event();

    void _data_channel_event();

    void _end_event();

    ros::NodeHandle _nh;

    ros::Subscriber _s;

    std::string _id;

    std::string _peer_id;

    QueueSizes _queue_sizes;

    bond::Bond _bond;

    MediaConstraints _sdp_constraints;

    std::vector<AudioSinkPtr> audio_sinks;

    std::vector<VideoRendererPtr> video_renderers;

    rtc::scoped_refptr<webrtc::MediaStreamInterface> _local_stream;

    rtc::scoped_refptr<webrtc::PeerConnectionInterface> _pc;

    ros::ServiceClient _ice_candidate_cli;

    ros::ServiceClient _sdp_offer_answer_cli;

    ros::ServiceClient _disconnect_cli;

    PeerConnectionObserver _pco;

    ObserverPtr _observer;

    rtc::scoped_refptr<CreateSessionDescriptionObserver> _csdo;

    rtc::scoped_refptr<SetSessionDescriptionObserver> _ssdo;

    bool _is_offerer;

    rtc::scoped_ptr<webrtc::SessionDescriptionInterface> _local_desc;

    bool _queue_remote_ice_candidates;

    std::list<IceCandidatePtr> _remote_ice_cadidates;

    typedef std::vector<ros_webrtc::DataChannel> DataChannelInits;

    typedef std::vector<DataChannel> DataChannels;

    DataChannels _dcs;

    typedef std::map<std::string, ros::ServiceClient> ServiceClients;

    ServiceClient _srv_cli;

    typedef std::list<AudioSinkPtr> AudioSinks;

    AudioSinks _audio_sinks;

    typedef std::list<VideoRendererPtr> VideoRenderers;

    VideoRenderers _video_renderers;

    ros::Publisher _epub;
};

typedef boost::shared_ptr<Session> SessionPtr;

typedef boost::shared_ptr<const Session> SessionConstPtr;


#endif /* ROS_WEBRTC_SESSION_H_ */
