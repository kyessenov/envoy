#pragma once

#include "envoy/config/core/v3/substitution_format_string.pb.h"
#include "envoy/config/core/v3/substitution_format_string.pb.validate.h"
#include "envoy/matcher/matcher.h"
#include "envoy/server/factory_context.h"

#include "source/common/formatter/substitution_format_string.h"
#include "source/common/protobuf/utility.h"
#include "source/extensions/matching/common_inputs/format_string/input.h"

namespace Envoy {
namespace Extensions {
namespace Matching {
namespace CommonInputs {
namespace FormatString {

template <class MatchingDataType>
class BaseFactory : public Matcher::DataInputFactory<MatchingDataType> {
public:
  Matcher::DataInputFactoryCb<MatchingDataType>
  createDataInputFactoryCb(const Protobuf::Message& proto_config,
                           Server::Configuration::ServerFactoryContext& factory_context) override {
    const auto& config =
        MessageUtil::downcastAndValidate<const envoy::config::core::v3::SubstitutionFormatString&>(
            proto_config, factory_context.messageValidationVisitor());
    Formatter::FormatterConstSharedPtr formatter =
        Formatter::SubstitutionFormatStringUtils::fromProtoConfig(config, factory_context);
    return [formatter]() { return std::make_unique<Input>(formatter); };
  }

  std::string name() const override { return "envoy.matching.inputs.format_string"; }

  ProtobufTypes::MessagePtr createEmptyConfigProto() override {
    return std::make_unique<envoy::config::core::v3::SubstitutionFormatString>();
  }
};

} // namespace FormatString
} // namespace CommonInputs
} // namespace Matching
} // namespace Extensions
} // namespace Envoy
