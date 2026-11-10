#include "xps_gzip.h"

void gzip_sink_handler(void *ptr);
void gzip_sink_close_handler(void *ptr);
void gzip_source_handler(void *ptr);
void gzip_source_close_handler(void *ptr);
void gzip_check_destroy(xps_gzip_t *gzip);

xps_gzip_t *xps_gzip_create(int level) {
    if (level < -1 || level > 9) {
        logger(LOG_ERROR, "xps_gzip_create()", "invalid compression level %d", level);
        return NULL;
    }

    xps_gzip_t *gzip = (xps_gzip_t*)calloc(1, sizeof(xps_gzip_t));
    if (gzip == NULL) {
        logger(LOG_ERROR, "xps_gzip_create()", "calloc() failed");
        return NULL;
    }

    gzip->source = xps_pipe_source_create((void *)gzip, gzip_source_handler, gzip_source_close_handler);
    if (gzip->source == NULL) {
        logger(LOG_ERROR, "xps_gzip_create()", "source creation failed");
        free(gzip);
        return NULL;
    }

    gzip->sink = xps_pipe_sink_create((void *)gzip, gzip_sink_handler, gzip_sink_close_handler);
    if (gzip->sink == NULL) {
        logger(LOG_ERROR, "xps_gzip_create()", "sink creation failed");
        xps_pipe_source_destroy(gzip->source);
        free(gzip);
        return NULL;
    }

    if (deflateInit2(&(gzip->stream), level, Z_DEFLATED, 16 + MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        logger(LOG_ERROR, "xps_gzip_create()", "deflateInit2 failed");
        xps_pipe_source_destroy(gzip->source);
        xps_pipe_sink_destroy(gzip->sink);
        free(gzip);
        return NULL;
    }
    
    gzip->sink->ready = true;
    gzip->transfer_buff = NULL;
    gzip->total_in_len = 0;
    gzip->total_out_len = 0;

    return gzip;
}

void xps_gzip_destroy(xps_gzip_t *gzip) {
    assert(gzip != NULL);
    
    deflateEnd(&(gzip->stream));
    
    // MATCHING YOUR HEADER: use handler_cb and close_cb
    if (gzip->source) {
        gzip->source->handler_cb = NULL;
        gzip->source->close_cb = NULL;
    }
    if (gzip->sink) {
        gzip->sink->handler_cb = NULL;
        gzip->sink->close_cb = NULL;
    }

    xps_pipe_source_destroy(gzip->source);  
    xps_pipe_sink_destroy(gzip->sink);

    if (gzip->transfer_buff) {
        xps_buffer_destroy(gzip->transfer_buff);
    }
    free(gzip);
}

void gzip_compress(xps_gzip_t *gzip, bool flush) {
    assert(gzip != NULL);
    
    xps_buffer_t *in_buff = NULL;

    if (!flush) {
        // MATCHING YOUR HEADER: use pipe->buff_list->len
        size_t available = gzip->sink->pipe->buff_list->len;
        if (available == 0) return;

        in_buff = xps_pipe_sink_read(gzip->sink, available);
        if (in_buff == NULL) {
            logger(LOG_ERROR, "gzip_compress()", "xps_pipe_sink_read failed");
            return;
        }
    } else {
        in_buff = xps_buffer_create(1, 0, NULL);
        if (in_buff == NULL) {
            logger(LOG_ERROR, "gzip_compress()", "xps_buffer_create() failed");
            return;
        }
    }

    gzip->stream.avail_in = in_buff->len;
    gzip->stream.next_in = (unsigned char *)in_buff->data;
    
    int flush_stream = flush ? Z_FINISH : Z_NO_FLUSH;
    xps_buffer_list_t *out_buff_list = xps_buffer_list_create();

    do {
        xps_buffer_t *out_buff = xps_buffer_create(DEFAULT_BUFFER_SIZE, 0, NULL);
        if (out_buff == NULL) {
            logger(LOG_ERROR, "gzip_compress()", "out_buff allocation failed");
            xps_buffer_list_destroy(out_buff_list);
            xps_buffer_destroy(in_buff);
            xps_gzip_destroy(gzip);
            return;
        }

        gzip->stream.avail_out = out_buff->size;
        gzip->stream.next_out = (unsigned char *)out_buff->data;

        int error = deflate(&(gzip->stream), flush_stream);
        assert(error != Z_STREAM_ERROR);

        out_buff->len = out_buff->size - gzip->stream.avail_out;

        if (out_buff->len > 0) {
            xps_buffer_list_append(out_buff_list, out_buff);
        } else {
            xps_buffer_destroy(out_buff);
        }

    } while (gzip->stream.avail_out == 0);

    assert(gzip->stream.avail_in == 0);

    xps_buffer_t *compressed_buff = NULL;
    if (out_buff_list->len > 0) {
        compressed_buff = xps_buffer_list_read(out_buff_list, out_buff_list->len);
    }
    
    xps_buffer_list_destroy(out_buff_list);

    if (!flush) {
        // MATCHING YOUR HEADER: use pipe->buff_list->len
        xps_pipe_sink_clear(gzip->sink, in_buff->len);
        gzip->total_in_len += in_buff->len;
    }
    xps_buffer_destroy(in_buff);

    gzip->transfer_buff = compressed_buff;
    gzip->source->ready = (compressed_buff != NULL);
    gzip->sink->ready = (compressed_buff == NULL);
}

void gzip_sink_handler(void *ptr) {
    xps_pipe_sink_t *sink = ptr;
    xps_gzip_t *gzip = sink->ptr;
    gzip_compress(gzip, false);
}

void gzip_sink_close_handler(void *ptr) {
    xps_pipe_sink_t *sink = ptr;
    xps_gzip_t *gzip = sink->ptr;
    gzip_compress(gzip, true);
}

void gzip_source_handler(void *ptr) {
    xps_pipe_source_t *source = ptr;
    xps_gzip_t *gzip = source->ptr;

    if (xps_pipe_source_write(gzip->source, gzip->transfer_buff) != OK) {
        logger(LOG_ERROR, "gzip_source_handler()", "source write failed");
        return;
    }

    gzip->total_out_len += gzip->transfer_buff->len;

    xps_buffer_destroy(gzip->transfer_buff);
    gzip->transfer_buff = NULL;

    gzip->source->ready = false;
    gzip->sink->ready = true;
    
    gzip_check_destroy(gzip);
}

void gzip_source_close_handler(void *ptr) {
    xps_pipe_source_t *source = ptr;
    xps_gzip_t *gzip = source->ptr;
    gzip_check_destroy(gzip);
}

void gzip_check_destroy(xps_gzip_t *gzip) {
    assert(gzip != NULL);
    
    // destroy if:
    // - downstream is closed, OR
    // - upstream is closed AND no pending data
    if (!gzip->source->active || (!gzip->sink->active && gzip->transfer_buff == NULL)) {
        xps_gzip_destroy(gzip);
    }
}