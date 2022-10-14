#pragma once

#include <atomic>
#include <mutex>

#include "envoy/access_log/access_log.h"
#include "api/http/golang/v3/golang.pb.h"
#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/common/grpc/context_impl.h"

#include "src/envoy/common/dso/dso.h"

#include "source/common/common/linked_object.h"
#include "source/common/buffer/watermark_buffer.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {

/**
 * Configuration for the HTTP golang extension filter.
 */
class FilterConfig : Logger::Loggable<Logger::Id::http> {
public:
  FilterConfig(
      const envoy::extensions::filters::http::golang::v3::Config& proto_config);

  const std::string& filter_chain() const { return filter_chain_; }
  const std::string& so_id() const { return so_id_; }
  const std::string& plugin_name() const { return plugin_name_; }
  uint64_t getConfigId();

private:
  const std::string filter_chain_;
  const std::string plugin_name_;
  const std::string so_id_;
  const Protobuf::Any plugin_config_;
  uint64_t config_id_ {0};
};

using FilterConfigSharedPtr = std::shared_ptr<FilterConfig>;

/**
 * Route configuration for the filter.
 */
class FilterConfigPerRoute : public Router::RouteSpecificFilterConfig {
public:
  // TODO
  FilterConfigPerRoute(const envoy::extensions::filters::http::golang::v3::ConfigsPerRoute&,
                       Server::Configuration::ServerFactoryContext&) {}

  ~FilterConfigPerRoute() override {}
};


/**
 * An enum specific for Golang status.
 */
enum class GolangStatus {
  Running,
  // Directi response
  DirectResponse,
  // Continue filter chain iteration.
  Continue,
  StopAndBuffer,
  StopAndBufferWatermark,
  StopNoBuffer,
};

/*
 * state
 */
enum class FilterState {
  WaitHeader,
  DoHeader,
  WaitData,
  WaitFullData,
  DoData,
  WaitTrailer,
  DoTrailer,
  Done,
  LocalReply,
};

/*
 * request phase
 */
enum class Phase {
  Init = 0,
  DecodeHeader,
  DecodeData,
  DecodeTrailer,
  EncodeHeader,
  EncodeData,
  EncodeTrailer,
  Done,
};

enum class DestroyReason {
  Normal,
  Terminate,
};

/**
 * See docs/configuration/http_filters/golang_extension_filter.rst
 */
class Filter : public Http::StreamFilter,
               public std::enable_shared_from_this<Filter>,
               Logger::Loggable<Logger::Id::http>,
               public AccessLog::Instance {
public:
  explicit Filter(Grpc::Context& context, FilterConfigSharedPtr config, uint64_t sid,
                  Dso::DsoInstance* dynamicLib)
      : config_(config), dynamicLib_(dynamicLib), context_(context), stream_id_(sid) {
  }

  // Http::StreamFilterBase
  void onDestroy() override;

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap&) override;
  void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override {
    decoder_callbacks_ = &callbacks;
  }

  // Http::StreamEncoderFilter
  Http::FilterHeadersStatus encode1xxHeaders(Http::ResponseHeaderMap&) override {
    return Http::FilterHeadersStatus::Continue;
  }
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance& data, bool end_stream) override;
  Http::FilterTrailersStatus encodeTrailers(Http::ResponseTrailerMap& trailers) override;
  Http::FilterMetadataStatus encodeMetadata(Http::MetadataMap&) override {
    return Http::FilterMetadataStatus::Continue;
  }

  void setEncoderFilterCallbacks(Http::StreamEncoderFilterCallbacks& callbacks) override {
    encoder_callbacks_ = &callbacks;
  }

  // AccessLog::Instance
  void log(const Http::RequestHeaderMap* request_headers,
           const Http::ResponseHeaderMap* response_headers,
           const Http::ResponseTrailerMap* response_trailers,
           const StreamInfo::StreamInfo& stream_info) override;

  void onStreamComplete() override;

  // create metadata for golang.extension namespace
  void addGolangMetadata(const std::string& k, const uint64_t v);

  void directResponse(Http::ResponseHeaderMapPtr&& headers, Buffer::Instance* body,
                      Http::ResponseTrailerMapPtr&& trailers);

  Event::Dispatcher& getDispatcher();
  bool hasDestroyed();

  static std::atomic<uint64_t> global_stream_id_;

  void continueStatus(GolangStatus status);

  void sendLocalReply(Http::Code response_code, absl::string_view body_text,
                      std::function<void(Http::ResponseHeaderMap& headers)> modify_headers,
                      Grpc::Status::GrpcStatus grpc_status,
                      absl::string_view details);

  absl::optional<absl::string_view> getHeader(absl::string_view key);
  void copyHeaders(GoString *goStrs, char *goBuf);
  void setHeader(absl::string_view key, absl::string_view value);
  void removeHeader(absl::string_view key);
  void copyBuffer(Buffer::Instance* buffer, char *data);
  void setBuffer(Buffer::Instance* buffer, absl::string_view &value);
  void copyTrailers(GoString *goStrs, char *goBuf);
  void setTrailer(absl::string_view key, absl::string_view value);

private:
  bool isDecodePhase();
  bool isHeaderPhase();
  bool isEmptyBuffer();

  void phaseGrow(int n=1);
  bool handleGolangStatusInHeader(GolangStatus status);
  bool handleGolangStatusInData(GolangStatus status);
  bool handleGolangStatusInTrailer(GolangStatus status);

  bool isThreadSafe();

  bool doHeaders(Http::RequestOrResponseHeaderMap& headers, bool end_stream);
  bool doData(Buffer::Instance&, bool);
  bool doDataGo(Buffer::Instance& data, bool end_stream);
  bool doTrailer(Http::HeaderMap& trailers);
  bool doTrailerGo(Http::HeaderMap& trailers);
  bool handleGolangStatus(GolangStatus status);

  void wantMoreData();
  void dataBufferFull();

  Buffer::InstancePtr createWatermarkBuffer();

  void continueStatusInternal(GolangStatus status);
  void commonContinue(bool is_decode);
  void continueData(bool is_decode);

  void onHeadersModified();

  const FilterConfigSharedPtr config_;
  Dso::DsoInstance* dynamicLib_;

  Phase phase_{Phase::Init};
  FilterState state_{FilterState::WaitHeader};

  Http::RequestOrResponseHeaderMap* headers_{nullptr}; 
  Http::HeaderMap* trailers_{nullptr}; 

  Buffer::InstancePtr data_buffer_;
  Buffer::OwnedImpl do_data_buffer_;

  bool end_stream_; // end_stream flag that c has received.
  bool do_end_stream_; // end_stream flag that go is proccessing.

  Http::StreamDecoderFilterCallbacks* decoder_callbacks_{nullptr};
  Http::StreamEncoderFilterCallbacks* encoder_callbacks_{nullptr};

  // TODO get all context
  Grpc::Context& context_;
  uint64_t cost_time_decode_{0};
  uint64_t cost_time_encode_{0};
  uint64_t cost_time_mem_{0};
  uint64_t stream_id_{0};

  httpRequest* req_{0};

  // lock for has_destroyed_,
  // to avoid race between envoy c thread and go thread (when calling back from go).
  // it should also be okay without this lock in most cases, just for extreme case.
  std::mutex mutex_{};
  bool has_destroyed_{false};
};

struct httpRequestInternal {
  uint64_t configId_;
  int phase_;
  std::weak_ptr<Filter> filter_;
  httpRequestInternal(std::weak_ptr<Filter> f) {
    filter_ = f;
  }
  std::weak_ptr<Filter> weakFilter() {
    return filter_;
  };
};

// used to count function execution time
template <typename T = std::chrono::microseconds> struct measure {
  template <typename F, typename... Args> static typename T::rep execution(F func, Args&&... args) {
    auto start = std::chrono::steady_clock::now();

    func(std::forward<Args>(args)...);

    auto duration = std::chrono::duration_cast<T>(std::chrono::steady_clock::now() - start);

    return duration.count();
  }
};

} // namespace Golang
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
