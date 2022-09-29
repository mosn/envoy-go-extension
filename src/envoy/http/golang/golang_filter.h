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
  // Continue filter chain iteration.
  Continue,
  // Directi response
  DirectResponse,
  // Need aysnc
  NeedAsync,
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
  Done,
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
    /*
    static std::atomic_flag first = ATOMIC_FLAG_INIT;
    // set callback function handler for async
    if (!first.test_and_set() && dynamicLib_ != NULL) {
      dynamicLib_->setPostDecodeCallbackFunc(postDecode);
      dynamicLib_->setPostEncodeCallbackFunc(postEncode);
    }
    */
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

  Http::RequestHeaderMap* getRequestHeaders();
  Http::ResponseHeaderMap* getResponseHeaders();
  Event::Dispatcher& getDispatcher();
  bool hasDestroyed();

/*
  static void postDecode(void* filter, Response resp);
  static void postEncode(void* filter, Response resp);
  */

  static std::atomic<uint64_t> global_stream_id_;

  void requestContinue(GolangStatus status);
  void responseContinue();
  absl::optional<absl::string_view> getRequestHeader(absl::string_view key);
  void copyRequestHeaders(_GoString_ *goStrs, char *goBuf);
  absl::optional<absl::string_view> getResponseHeader(absl::string_view key);
  void copyResponseHeaders(_GoString_ *goStrs, char *goBuf);
  void setResponseHeader(absl::string_view key, absl::string_view value);
  void copyBuffer(Buffer::Instance* buffer, char *data);
  void setBuffer(Buffer::Instance* buffer, absl::string_view &value);

private:
  bool isThreadSafe();

  bool doData(Buffer::Instance&, bool);
  bool handleGolangStatus(GolangStatus status);

  void wantData();
  void dataBufferFull();

  Buffer::InstancePtr createWatermarkBuffer();

  /*
  // build golang request header
  bool buildGolangRequestHeaders(Request& req, Http::HeaderMap& headers);
  // build golang request data
  void buildGolangRequestBodyDecode(Request& req, const Buffer::Instance& data);
  void buildGolangRequestBodyEncode(Request& req, const Buffer::Instance& data);
  // build golang request trailer
  bool buildGolangRequestTrailers(Request& req, Http::HeaderMap& trailers);
  // build golang request filter chain
  void buildGolangRequestFilterChain(Request& req, const std::string& filter_chain);
  // build golang request remote address
  void buildGolangRequestRemoteAddress(Request& req);
  */

  void onHeadersModified();
  /*
  static void buildHeadersOrTrailers(Http::HeaderMap& dheaders, NonConstString* sheaders);
  */

  static std::string buildBody(const Buffer::Instance* buffered, const Buffer::Instance& last);

/*
  static void freeCharPointer(NonConstString* p);
  static void freeCharPointerArray(NonConstString* p);
  static void freeReqAndResp(Request& req, Response& resp);
  static void initReqAndResp(Request& req, Response& resp, size_t headerSize, size_t TrailerSize);
  */

  const FilterConfigSharedPtr config_;
  Dso::DsoInstance* dynamicLib_;

  FilterState state_{FilterState::WaitHeader};

  Buffer::InstancePtr request_data_buffer_;
  Buffer::InstancePtr response_data_buffer_;

  Buffer::OwnedImpl request_do_data_buffer_;
  bool end_stream_;

  Http::StreamDecoderFilterCallbacks* decoder_callbacks_{nullptr};
  Http::StreamEncoderFilterCallbacks* encoder_callbacks_{nullptr};
  Http::RequestHeaderMap* request_headers_{nullptr};
  Buffer::Instance* request_data_{nullptr};
  Http::ResponseHeaderMap* response_headers_{nullptr};
  std::string req_body_{};
  std::string resp_body_{};
  // TODO get all context
  Grpc::Context& context_;
  bool has_async_task_{false};
  bool decode_goextension_executed_{false};
  bool encode_goextension_executed_{false};
  uint64_t cost_time_decode_{0};
  uint64_t cost_time_encode_{0};
  uint64_t cost_time_mem_{0};
  uint64_t stream_id_{0};

  uint64_t ptr_holder_{0};

  // lock for has_destroyed_,
  // to avoid race between envoy c thread and go thread (when calling back from go).
  // it should also be okay without this lock in most cases, just for extreme case.
  std::mutex mutex_{};
  bool has_destroyed_{false};
};

class FilterWeakPtrHolder {
public:
  FilterWeakPtrHolder(std::weak_ptr<Filter> ptr) : ptr_(ptr) {}
  ~FilterWeakPtrHolder(){};
  std::weak_ptr<Filter>& get() { return ptr_; }

private:
  std::weak_ptr<Filter> ptr_{};
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
