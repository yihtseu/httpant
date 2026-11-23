export module httpant.quic:credentials;
import std;
import httpant.dependencies.msquic;

export namespace http::quic {
    namespace detail {
        template <template <class> class Queue, template <class> class Local> struct state;
    }

    class credentials {
        bool server_;
        std::string certificate_, key_, ca_;
        explicit credentials(bool server) : server_(server) {}

    public:
        static auto server(const std::string& certificate, const std::string& key) -> credentials {
            credentials result{true};
            result.certificate_ = certificate;
            result.key_ = key;
            return result;
        }
        static auto client(const std::string& ca = {}) -> credentials {
            credentials result{false};
            result.ca_ = ca;
            return result;
        }
        auto is_server() const noexcept -> bool { return server_; }

    private:
        template <template <class> class Queue, template <class> class Local> friend struct detail::state;
        auto load(const MsQuicApi& api, HQUIC configuration) const noexcept -> QUIC_STATUS {
            QUIC_CREDENTIAL_CONFIG config{};
            QUIC_CERTIFICATE_FILE file{key_.c_str(), certificate_.c_str()};
            config.Type = server_ ? QUIC_CREDENTIAL_TYPE_CERTIFICATE_FILE : QUIC_CREDENTIAL_TYPE_NONE;
            config.Flags = server_ ? QUIC_CREDENTIAL_FLAG_NONE : QUIC_CREDENTIAL_FLAG_CLIENT;
            if (server_)
                config.CertificateFile = &file;
            if (!ca_.empty()) {
                config.Flags = static_cast<QUIC_CREDENTIAL_FLAGS>(config.Flags | QUIC_CREDENTIAL_FLAG_SET_CA_CERTIFICATE_FILE);
                config.CaCertificateFile = ca_.c_str();
            }
            return api.ConfigurationLoadCredential(configuration, &config);
        }
    };
} // namespace http::quic
