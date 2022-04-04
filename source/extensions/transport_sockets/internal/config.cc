#include "source/extensions/transport_sockets/internal/config.h"

#include "envoy/extensions/transport_sockets/internal/v3/internal_upstream.pb.validate.h"
#include "envoy/registry/registry.h"
#include "envoy/server/transport_socket_config.h"

#include "source/common/config/utility.h"
#include "source/extensions/transport_sockets/internal/internal.h"

namespace Envoy {
namespace Extensions {
namespace TransportSockets {
namespace Internal {

namespace {

class InternalUpstreamConfigFactory
    : public Server::Configuration::UpstreamTransportSocketConfigFactory {
public:
  std::string name() const override { return "envoy.transport_sockets.internal_upstream"; }
  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<
        envoy::extensions::transport_sockets::internal::v3::InternalUpstreamTransport>();
  }
  Network::TransportSocketFactoryPtr createTransportSocketFactory(
      const Protobuf::Message& config,
      Server::Configuration::TransportSocketFactoryContext& context) override {
    const auto& outer_config = MessageUtil::downcastAndValidate<
        const envoy::extensions::transport_sockets::internal::v3::InternalUpstreamTransport&>(
        config, context.messageValidationVisitor());
    auto& inner_config_factory = Envoy::Config::Utility::getAndCheckFactory<
        Server::Configuration::UpstreamTransportSocketConfigFactory>(
        outer_config.transport_socket());
    ProtobufTypes::MessagePtr inner_factory_config =
        Envoy::Config::Utility::translateToFactoryConfig(outer_config.transport_socket(),
                                                         context.messageValidationVisitor(),
                                                         inner_config_factory);
    auto inner_transport_factory =
        inner_config_factory.createTransportSocketFactory(*inner_factory_config, context);
    return std::make_unique<InternalSocketFactory>(context, outer_config,
                                                   std::move(inner_transport_factory));
  }
};

} // namespace

Config::Config(const envoy::extensions::transport_sockets::internal::v3::InternalUpstreamTransport&
                   config_proto,
               Stats::Scope&) {
  for (const auto& metadata : config_proto.passthrough_metadata()) {
    MetadataKind kind;
    switch (metadata.kind().kind_case()) {
    case envoy::type::metadata::v3::MetadataKind::KindCase::kHost:
      kind = MetadataKind::Host;
      break;
    case envoy::type::metadata::v3::MetadataKind::KindCase::kCluster:
      kind = MetadataKind::Cluster;
      break;
    default:
      throw EnvoyException(
          absl::StrCat("metadata type is not supported: ", metadata.kind().DebugString()));
    }
    metadata_sources_.push_back(MetadataSource(kind, metadata.name()));
  }
  for (const auto& filter_state_object : config_proto.passthrough_filter_state_objects()) {
    filter_state_names_.push_back(filter_state_object.name());
  }
}
std::unique_ptr<envoy::config::core::v3::Metadata>
Config::extractMetadata(const Upstream::HostDescriptionConstSharedPtr& host) const {
  if (metadata_sources_.empty()) {
    return nullptr;
  }
  auto metadata = std::make_unique<envoy::config::core::v3::Metadata>();
  for (const auto& source : metadata_sources_) {
    switch (source.kind_) {
    case MetadataKind::Host: {
      if (host->metadata()->filter_metadata().contains(source.name_)) {
        (*metadata->mutable_filter_metadata())[source.name_] =
            host->metadata()->filter_metadata().at(source.name_);
      }
      break;
    }
    case MetadataKind::Cluster: {
      if (host->cluster().metadata().filter_metadata().contains(source.name_)) {
        (*metadata->mutable_filter_metadata())[source.name_] =
            host->cluster().metadata().filter_metadata().at(source.name_);
      }
      break;
    }
    default:
      PANIC("not reached");
    }
  }
  return metadata;
}

std::unique_ptr<IoSocket::UserSpace::FilterStateObjects>
Config::extractFilterState(const StreamInfo::FilterStateSharedPtr& filter_state) const {
  if (filter_state_names_.empty()) {
    return nullptr;
  }
  auto filter_state_objects = std::make_unique<IoSocket::UserSpace::FilterStateObjects>();
  for (const auto& name : filter_state_names_) {
    try {
      auto object = filter_state->getDataSharedMutableGeneric(name);
      filter_state_objects->emplace_back(name, object);
    } catch (const EnvoyException& e) {
    }
  }
  return filter_state_objects;
}

InternalSocketFactory::InternalSocketFactory(
    Server::Configuration::TransportSocketFactoryContext& context,
    const envoy::extensions::transport_sockets::internal::v3::InternalUpstreamTransport& config,
    Network::TransportSocketFactoryPtr&& inner_factory)
    : PassthroughFactory(std::move(inner_factory)) {
  config_ = std::make_shared<Config>(config, context.scope());
}

Network::TransportSocketPtr InternalSocketFactory::createTransportSocket(
    Network::TransportSocketOptionsConstSharedPtr options) const {
  auto inner_socket = transport_socket_factory_->createTransportSocket(options);
  if (inner_socket == nullptr) {
    return nullptr;
  }
  std::unique_ptr<envoy::config::core::v3::Metadata> extracted_metadata;
  if (options && options->host()) {
    extracted_metadata = config_->extractMetadata(options->host());
  }
  std::unique_ptr<IoSocket::UserSpace::FilterStateObjects> extracted_filter_state;
  if (options && options->filterState()) {
    extracted_filter_state = config_->extractFilterState(options->filterState());
  }
  return std::make_unique<InternalSocket>(std::move(inner_socket), std::move(extracted_metadata),
                                          std::move(extracted_filter_state));
}

REGISTER_FACTORY(InternalUpstreamConfigFactory,
                 Server::Configuration::UpstreamTransportSocketConfigFactory);

} // namespace Internal
} // namespace TransportSockets
} // namespace Extensions
} // namespace Envoy
