idf_component_register(
    SRCS "captive_portal.c" "dns_responder.c" "portal_pages.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi esp_netif esp_http_server lwip esp_hw_support identity settings
)
