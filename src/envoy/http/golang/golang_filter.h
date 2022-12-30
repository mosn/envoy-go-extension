#pragma once

#include <atomic>
#include <mutex>

#include "envoy/access_log/access_log.h"
#include "api/http/golang/v3/golang.pb.h"
#include "envoy/http/filter.h"
#include "envoy/upstream/cluster_manager.h"

#include "source/common/http/utility.h"
#include "source/common/grpc/context_impl.h"

#include "source/common/common/linked_object.h"
#include "source/common/buffer/watermark_buffer.h"

#include "src/envoy/common/dso/dso.h"
#include "src/envoy/http/golang/processor_state.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace Golang {

/**
 * Configuration for the HTTP golang extension filter.
 */
class FilterConfig : Logger::Loggable<Logger::Id::http> {
public:
  FilterConfig(const envoy::extensions::filters::http::golang::v3::Config& proto_config);
  ~FilterConfig() {
    // TODO: delete config in Go
  }

  const std::string& filter_chain() const { return filter_chain_; }
  const std::string& so_id() const { return so_id_; }
  const std::string& plugin_name() const { return plugin_name_; }
  uint64_t getConfigId();

private:
  const std::string filter_chain_;
  const std::string plugin_name_;
  const std::string so_id_;
  const Protobuf::Any plugin_config_;
  uint64_t config_id_{0};
};

using FilterConfigSharedPtr = std::shared_ptr<FilterConfig>;

class RoutePluginConfig : Logger::Loggable<Logger::Id::http> {
public:
  RoutePluginConfig(const envoy::extensions::filters::http::golang::v3::RouterPlugin& config)
      : plugin_config_(config.config()) {
    ENVOY_LOG(debug, "initilizing golang filter route plugin config, type_url: {}",
              config.config().type_url());
  };
  ~RoutePluginConfig() {
    // TODO: delete plugin config in Go
  }
  uint64_t getMergedConfigId(uint64_t parent_id, std::string so_id);

private:
  const Protobuf::Any plugin_config_;
  uint64_t config_id_{0};
  uint64_t merged_config_id_{0};
};

/**
 * Route configuration for the filter.
 */
class FilterConfigPerRoute : public Router::RouteSpecificFilterConfig,
                             Logger::Loggable<Logger::Id::http> {
public:
  FilterConfigPerRoute(const envoy::extensions::filters::http::golang::v3::ConfigsPerRoute&,
                       Server::Configuration::ServerFactoryContext&);
  uint64_t getPluginConfigId(uint64_t parent_id, std::string plugin_name, std::string so_id) const;

  ~FilterConfigPerRoute() {
    for (auto it = plugins_config_.cbegin(); it != plugins_config_.cend(); ++it) {
      delete it->second;
    }
  }

private:
  std::map<std::string, RoutePluginConfig*> plugins_config_;
};

enum class DestroyReason {
  Normal,
  Terminate,
};

enum class StringValue {
  RouteName = 1,
};

struct httpRequestInternal;

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
      : config_(config), dynamicLib_(dynamicLib), decoding_state_(*this), encoding_state_(*this),
        context_(context), stream_id_(sid) {
    (void)context_;
    (void)stream_id_;
  }

  // Http::StreamFilterBase
  void onDestroy() override;
  Http::LocalErrorStatus onLocalReply(const LocalReplyData&) override;

  // Http::StreamDecoderFilter
  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers,
                                          bool end_stream) override;
  Http::FilterDataStatus decodeData(Buffer::Instance&, bool) override;
  Http::FilterTrailersStatus decodeTrailers(Http::RequestTrailerMap&) override;
  void setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) override {
    decoding_state_.setDecoderFilterCallbacks(callbacks);
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
    encoding_state_.setEncoderFilterCallbacks(callbacks);
  }

  // AccessLog::Instance
  void log(const Http::RequestHeaderMap* request_headers,
           const Http::ResponseHeaderMap* response_headers,
           const Http::ResponseTrailerMap* response_trailers,
           const StreamInfo::StreamInfo& stream_info) override;

  void onStreamComplete() override;

  // create metadata for golang.extension namespace
  void addGolangMetadata(const std::string& k, const uint64_t v);

  static std::atomic<uint64_t> global_stream_id_;

  int continueStatus(GolangStatus status);

  int sendLocalReply(Http::Code response_code, absl::string_view body_text,
                     std::function<void(Http::ResponseHeaderMap& headers)> modify_headers,
                     Grpc::Status::GrpcStatus grpc_status, absl::string_view details);

  int getHeader(absl::string_view key, GoString* goValue);
  int copyHeaders(GoString* goStrs, char* goBuf);
  int setHeader(absl::string_view key, absl::string_view value, headerAction act);
  int removeHeader(absl::string_view key);
  int copyBuffer(Buffer::Instance* buffer, char* data);
  int setBufferHelper(Buffer::Instance* buffer, absl::string_view& value, bufferAction action);
  int copyTrailers(GoString* goStrs, char* goBuf);
  int setTrailer(absl::string_view key, absl::string_view value);
  int getStringValue(int id, GoString* valueStr);
  int getDynamicMetadata(std::string filter_name, GoSlice* bufSlice);
  int setDynamicMetadata(std::string filter_name, std::string key, absl::string_view bufStr);

private:
  ProcessorState& getProcessorState();

  bool doHeaders(ProcessorState& state, Http::RequestOrResponseHeaderMap& headers, bool end_stream);
  GolangStatus doHeadersGo(ProcessorState& state, Http::RequestOrResponseHeaderMap& headers,
                           bool end_stream);
  bool doData(ProcessorState& state, Buffer::Instance&, bool);
  bool doDataGo(ProcessorState& state, Buffer::Instance& data, bool end_stream);
  bool doTrailer(ProcessorState& state, Http::HeaderMap& trailers);
  bool doTrailerGo(ProcessorState& state, Http::HeaderMap& trailers);

  uint64_t getMergedConfigId(ProcessorState& state);

  void continueEncodeLocalReply(ProcessorState& state);
  void continueStatusInternal(GolangStatus status);
  void continueData(ProcessorState& state);

  void onHeadersModified();

  void sendLocalReplyInternal(Http::Code response_code, absl::string_view body_text,
                              std::function<void(Http::ResponseHeaderMap& headers)> modify_headers,
                              Grpc::Status::GrpcStatus grpc_status, absl::string_view details);

  void getDynamicMetadataAsync(std::string filter_name, GoSlice* bufSlice);
  void setDynamicMetadataInternal(ProcessorState& state, std::string filter_name, std::string key,
                                  const absl::string_view& bufStr);

  const FilterConfigSharedPtr config_;
  Dso::DsoInstance* dynamicLib_;

  Http::RequestOrResponseHeaderMap* headers_{nullptr};
  Http::HeaderMap* trailers_{nullptr};

  // save temp values from local reply
  Http::RequestOrResponseHeaderMap* local_headers_{nullptr};
  Http::HeaderMap* local_trailers_{nullptr};

  // The state of the filter on both the encoding and decoding side.
  DecodingProcessorState decoding_state_;
  EncodingProcessorState encoding_state_;

  // TODO get all context
  Grpc::Context& context_;
  uint64_t cost_time_decode_{0};
  uint64_t cost_time_encode_{0};
  uint64_t cost_time_mem_{0};
  uint64_t stream_id_{0};

  httpRequestInternal* req_{0};

  // lock for has_destroyed_,
  // to avoid race between envoy c thread and go thread (when calling back from go).
  // it should also be okay without this lock in most cases, just for extreme case.
  std::mutex mutex_{};
  bool has_destroyed_{false};

  // other filter trigger sendLocalReply during go processing in async.
  // will wait go return before continue.
  // this variable is read/write in safe thread, do no need lock.
  bool local_reply_waiting_go_{false};

  // the filter enter encoding phase
  bool enter_encoding_{false};
};

// Go code only touch the fields in httpRequest
struct httpRequestInternal : httpRequest {
  std::weak_ptr<Filter> filter_;
  // anchor a string temporarily, make sure it won't be freed before copied to Go.
  std::string strValue;
  httpRequestInternal(std::weak_ptr<Filter> f) {
    filter_ = f;
    waitSema = 0;
  }
  std::weak_ptr<Filter> weakFilter() { return filter_; }
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
