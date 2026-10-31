/* Respondedor DNS minimo para portal cautivo: toda consulta A recibe 192.168.4.1. */
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include "portal_internal.h"

static const char *TAG = "dns";

static TaskHandle_t s_task;
static int s_sock = -1;
static volatile bool s_run;

static void dns_task(void *arg)
{
    (void)arg;
    uint8_t buf[512];
    struct sockaddr_in from;
    socklen_t from_len;
    const uint8_t answer_ip[4] = { 192, 168, 4, 1 };

    while (s_run) {
        from_len = sizeof(from);
        int n = recvfrom(s_sock, buf, sizeof(buf) - 16, 0, (struct sockaddr *)&from, &from_len);
        if (n < 12) {
            if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && s_run) {
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            continue;
        }
        /* cabecera: ID | flags | QDCOUNT | ANCOUNT | NSCOUNT | ARCOUNT */
        if (buf[2] & 0x80) continue; /* es una respuesta, no una consulta */
        buf[2] = 0x81; buf[3] = 0x80;  /* respuesta, recursion disponible */
        buf[6] = 0; buf[7] = 1;        /* ANCOUNT = 1 */
        buf[8] = buf[9] = buf[10] = buf[11] = 0;

        /* saltar QNAME + QTYPE + QCLASS de la primera pregunta */
        int p = 12;
        while (p < n && buf[p] != 0) p += buf[p] + 1;
        p += 5;
        if (p > n) continue;

        /* respuesta: puntero al nombre (0xC00C), tipo A, clase IN, TTL 60, 4 bytes */
        const uint8_t rr[] = { 0xC0, 0x0C, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x04 };
        memcpy(buf + p, rr, sizeof(rr));
        memcpy(buf + p + sizeof(rr), answer_ip, 4);
        sendto(s_sock, buf, p + sizeof(rr) + 4, 0, (struct sockaddr *)&from, from_len);
    }
    s_task = NULL;
    vTaskDelete(NULL);
}

esp_err_t dns_responder_start(void)
{
    if (s_task) return ESP_OK;
    s_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s_sock < 0) return ESP_FAIL;
    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(53), .sin_addr.s_addr = INADDR_ANY };
    if (bind(s_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }
    struct timeval tv = { .tv_sec = 0, .tv_usec = 200 * 1000 }; /* para poder parar la tarea */
    setsockopt(s_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    s_run = true;
    if (xTaskCreatePinnedToCore(dns_task, "dns_task", 3072, NULL, 4, &s_task, 0) != pdPASS) {
        close(s_sock);
        s_sock = -1;
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "respondedor DNS activo");
    return ESP_OK;
}

void dns_responder_stop(void)
{
    s_run = false;
    for (int i = 0; i < 20 && s_task; i++) vTaskDelay(pdMS_TO_TICKS(50));
    if (s_sock >= 0) {
        close(s_sock);
        s_sock = -1;
    }
}
