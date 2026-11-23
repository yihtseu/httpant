import std;
import httpant.quic;

auto test_credentials(std::string_view input) -> std::pair<http::quic::credentials, http::quic::credentials> {
    const std::filesystem::path directory{input};
    const auto certificate = (directory / "quic-cert.pem").string();
    const auto key = (directory / "quic-key.pem").string();
    return {http::quic::credentials::server(certificate, key), http::quic::credentials::client(certificate)};
}
