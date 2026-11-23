export module httpant.quic:credentials;
import std;
import httpant.dependencies.msquic;

export namespace http::quic {
    namespace detail {
        template <template <class> class Queue, template <class> class Local> struct state;
    }

    enum class certificate_store { current_user, local_machine };

    class credentials {
        std::optional<std::array<std::uint8_t, 20>> thumbprint_;
        certificate_store store_ = certificate_store::current_user;
        credentials() = default;

    public:
        static auto server(std::string_view thumbprint, certificate_store store = certificate_store::current_user) -> credentials {
            credentials result;
            auto& bytes = result.thumbprint_.emplace();
            if (thumbprint.size() != 2 * bytes.size())
                throw std::invalid_argument("quic: certificate thumbprint must contain 40 hexadecimal digits");
            for (auto& byte : bytes) {
                const auto* end = thumbprint.data() + 2;
                const auto [parsed, error] = std::from_chars(thumbprint.data(), end, byte, 16);
                if (error != std::errc{} || parsed != end)
                    throw std::invalid_argument("quic: certificate thumbprint must contain 40 hexadecimal digits");
                thumbprint.remove_prefix(2);
            }
            result.store_ = store;
            return result;
        }
        static auto client() -> credentials { return {}; }
        auto is_server() const noexcept -> bool { return thumbprint_.has_value(); }

    private:
        template <template <class> class Queue, template <class> class Local> friend struct detail::state;
        auto load(const MsQuicApi& api, HQUIC configuration) const noexcept -> QUIC_STATUS {
            QUIC_CREDENTIAL_CONFIG config{};
            if (!thumbprint_) {
                config.Type = QUIC_CREDENTIAL_TYPE_NONE;
                config.Flags = QUIC_CREDENTIAL_FLAG_CLIENT;
                return api.ConfigurationLoadCredential(configuration, &config);
            }

            QUIC_CERTIFICATE_HASH_STORE certificate{
                .Flags = store_ == certificate_store::local_machine ? QUIC_CERTIFICATE_HASH_STORE_FLAG_MACHINE_STORE
                                                                    : QUIC_CERTIFICATE_HASH_STORE_FLAG_NONE,
                .StoreName = "MY",
            };
            std::ranges::copy(*thumbprint_, certificate.ShaHash);
            config.Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_HASH_STORE;
            config.CertificateHashStore = &certificate;
            return api.ConfigurationLoadCredential(configuration, &config);
        }
    };
} // namespace http::quic
