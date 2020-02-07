#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <inaddr.h>
#else
#include <netinet/ip.h>
#endif

#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "clock.hh"
#include "frame.hh"
#include "rtcp.hh"
#include "runner.hh"
#include "socket.hh"
#include "util.hh"

namespace kvz_rtp {

    class dispatcher;
    class frame_queue;

    class connection : public runner {
        public:
            connection(rtp_format_t fmt, bool reader);
            connection(std::string addr, int src_port, int dst_port, rtp_format_t fmt, int flags);
            connection(kvz_rtp::connection *conn);
            virtual ~connection();

            uint16_t     get_sequence() const;
            uint32_t     get_ssrc()     const;
            rtp_format_t get_payload()  const;

            rtp_error_t init();

            socket&  get_socket();
            socket_t get_raw_socket();

            void set_payload(rtp_format_t fmt);
            void set_ssrc(uint32_t ssrc);
            void set_frame_queue(kvz_rtp::frame_queue *fqueue);

            /* Functions for increasing various RTP statistics
             * Overloaded functions without parameters increase the counter by 1
             *
             * Functions that take SSRC are for updating receiver statistics */
            void inc_rtp_sequence(size_t n);
            void inc_sent_bytes(size_t n);
            void inc_sent_pkts(size_t n);
            void inc_sent_pkts();
            void inc_rtp_sequence();

            /* See RTCP->update_receiver_stats() for documentation */
            rtp_error_t update_receiver_stats(kvz_rtp::frame::rtp_frame *frame);

            /* Config setters and getter, used f.ex. for Opus
             *
             * Return nullptr if the media doesn't have a config
             * Otherwise return void pointer to config (caller must do the cast) */
            void set_config(void *config);
            void *get_config();

            /* helper function fill the rtp header to allocated buffer,
             * caller must make sure that the buffer is at least 12 bytes long */
            void fill_rtp_header(uint8_t *buffer);

            void update_rtp_sequence(uint8_t *buffer);

            /* Set clock rate for RTP timestamp in Hz
             * This must be set, otherwise the timestamps won't be correct */
            void set_clock_rate(uint32_t clock_rate);

            /* Create RTCP instance for this connection
             *
             * This instance listens to src_port for incoming RTCP reports and sends
             * repots about this session to dst_addr:dst_port every N seconds (see RFC 3550) */
            rtp_error_t create_rtcp(std::string dst_addr, int dst_port, int src_port);

            /* Get pointer to frame queue
             *
             * Return pointer to frame queue for writers
             * Return nullptr for readers */
            kvz_rtp::frame_queue *get_frame_queue();

            /* Get pointer to dispatcher
             *
             * Return pointer to dispatcher for medias that use dispatcher
             * Return nullptr otherwise */
            kvz_rtp::dispatcher *get_dispatcher();

            /* Install deallocation hook for the transaction's data payload
             *
             * When SCD has finished processing the request,
             * in deinit_transaction(), it will calls this hook if it has been set. */
            void install_dealloc_hook(void (*dealloc_hook)(void *));

            /* Return pointer to RTCP object if RTCP has been enabled
             * Otherwise return nullptr */
            kvz_rtp::rtcp *get_rtcp();

            /* Get the connection-specific context configuration 
             * Used by both readers and writers */
            rtp_ctx_conf_t& get_ctx_conf();

            /* Enable some feature of kvzRTP that does not require extra configuration. 
             *
             * F.ex: writer->configure(RCE_SYSTEM_CALL_DISPATCHER);
             *
             * Return RTP_OK on success 
             * Return RTP_INVALID_VALUE if "flag" is not valid configuration option */
            rtp_error_t configure(int flag);

            /* Enable some feature of kvzRTP with additional parameter
             *
             * F.ex: writer->configure(RCC_UDP_BUF_SIZE, 4 * 1024 * 1024);
             *
             * Return RTP_OK on success 
             * Return RTP_INVALID_VALUE if "flag" is not valid configuration option */
            rtp_error_t configure(int flag, ssize_t value);

            /* If user does not want to use ZRTP for key management but wishes to do it
             * by himself, a master key and its length must be provided 
             *
             * Return RTP_OK on success
             * Return RTP_INVALID_VALUE if "key" or keylen are invalid
             * Return RTP_NOT_SUPPORTED if RCE_SRTP_KMNGMNT_USER is not set and set_srtp_key() is called */
            rtp_error_t set_srtp_key(uint8_t *key, size_t keylen);

            /* Called internally by socket.cc to retrieve master key user provided 
             * when initializing SRTP context */
            std::pair<uint8_t *, size_t>& get_srtp_key();

        protected:
            void *config_;
            uint32_t id_;

            int src_port_;
            int dst_port_;
            sockaddr_in addr_out_;
            std::string addr_;
            rtp_format_t fmt_;
            int flags_;

            kvz_rtp::socket socket_;
            kvz_rtp::rtcp *rtcp_;

        private:
            bool reader_;

            /* RTP */
            uint16_t rtp_sequence_;
            uint32_t rtp_ssrc_;
            uint32_t rtp_timestamp_;
            uint64_t wc_start_;
            rtp_format_t rtp_payload_;
            kvz_rtp::clock::hrc::hrc_t wc_start_2;
            uint32_t clock_rate_;

            rtp_ctx_conf_t conf_;

            /* After creation in writer.cc, pointer to frame queue is transferred
             * to conn.cc so we can get rid of the dynamic cast in push_fram() */
            kvz_rtp::frame_queue *fqueue_;

            /* Store key temporarily here before SRTP context is initialized */
            std::pair<uint8_t *, size_t> srtp_key_;
    };
};
