import std;
import httpant.quic;

auto test_credentials(std::string_view thumbprint) -> std::pair<http::quic::credentials, http::quic::credentials> {
    return {http::quic::credentials::server(thumbprint), http::quic::credentials::client()};
}
