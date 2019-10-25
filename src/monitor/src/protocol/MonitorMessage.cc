/* -------------------------------------------------------------------------- */
/* Copyright 2002-2019, OpenNebula Project, OpenNebula Systems                */
/*                                                                            */
/* Licensed under the Apache License, Version 2.0 (the "License"); you may    */
/* not use this file except in compliance with the License. You may obtain    */
/* a copy of the License at                                                   */
/*                                                                            */
/* http://www.apache.org/licenses/LICENSE-2.0                                 */
/*                                                                            */
/* Unless required by applicable law or agreed to in writing, software        */
/* distributed under the License is distributed on an "AS IS" BASIS,          */
/* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   */
/* See the License for the specific language governing permissions and        */
/* limitations under the License.                                             */
/* -------------------------------------------------------------------------- */

#include <openssl/bio.h>
#include <openssl/evp.h>

#include <zlib.h>

#include <sstream>

#include "MonitorMessage.h"

const EString<MonitorMessage::Type> MonitorMessage::type_str({
    {"MONITOR_VM", Type::MONITOR_VM},
    {"MONITOR_HOST", Type::MONITOR_HOST},
    {"SYSTEM_HOST", Type::SYSTEM_HOST},
    {"STATE_VM", Type::STATE_VM},
    {"UNDEFINED", Type::UNDEFINED}
});

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int MonitorMessage::parse_from(const std::string& input)
{
    std::istringstream is(input);
    std::string buffer, payloaz;

    _type = Type::UNDEFINED;
    _payload.clear();

    if (!is.good())
    {
        return -1;
    }

    is >> buffer >> std::ws;

    _type = type_str._from_str(buffer.c_str());

    if ( !is.good() || _type == Type::UNDEFINED )
    {
        return -1;
    }

    buffer.clear();

    is >> buffer >> std::ws;

    base64_decode(buffer, payloaz);

    if ( zlib_decompress(payloaz, _payload) == -1 )
    {
        _type = Type::UNDEFINED;
        _payload.clear();

        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int MonitorMessage::write_to(std::string& out) const
{
    out.clear();

    std::string payloaz;
    std::string payloaz64;

    if (zlib_compress(_payload, payloaz) == -1)
    {
        return -1;
    }

    if ( base64_encode(payloaz, payloaz64) == -1)
    {
        return -1;
    }

    out = type_str._to_str(_type);
    out += ' ';
    out += payloaz64;
    out += '\n';

    return 0;
}

/* ************************************************************************** */
/* ************************************************************************** */
/* Message helper functions                                                   */
/* ************************************************************************** */
/* ************************************************************************** */

void MonitorMessage::base64_decode(const std::string& in, std::string& out)
{
    BIO * bio_64  = BIO_new(BIO_f_base64());

    BIO * bio_mem_in  = BIO_new(BIO_s_mem());
    BIO * bio_mem_out = BIO_new(BIO_s_mem());

    bio_64 = BIO_push(bio_64, bio_mem_in);

    BIO_set_flags(bio_64, BIO_FLAGS_BASE64_NO_NL);

    BIO_write(bio_mem_in, in.c_str(), in.length());

    char inbuf[512];
    int  inlen;

    while ((inlen = BIO_read(bio_64, inbuf, 512)) > 0)
    {
        BIO_write(bio_mem_out, inbuf, inlen);
    }

    char * decoded_c;

    long int size = BIO_get_mem_data(bio_mem_out, &decoded_c);

    out.assign(decoded_c, size);

    BIO_free_all(bio_64);
    BIO_free_all(bio_mem_out);
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int MonitorMessage::base64_encode(const std::string& in, std::string &out)
{
    BIO * bio_64  = BIO_new(BIO_f_base64());
    BIO * bio_mem = BIO_new(BIO_s_mem());

    BIO_push(bio_64, bio_mem);

    BIO_set_flags(bio_64, BIO_FLAGS_BASE64_NO_NL);

    BIO_write(bio_64, in.c_str(), in.length());

    if (BIO_flush(bio_64) != 1)
    {
        return -1;
    }

    char * encoded_c;

    long int size = BIO_get_mem_data(bio_mem, &encoded_c);

    out.assign(encoded_c, size);

    BIO_free_all(bio_64);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

/**
 *  Buffer length for zlib inflate/deflate
 */
#define ZBUFFER 16384

int MonitorMessage::zlib_decompress(const std::string& in, std::string& out)
{
    if ( in.empty() )
    {
        return -1;
    }

    z_stream zs;

    zs.zalloc = Z_NULL;
    zs.zfree  = Z_NULL;
    zs.opaque = Z_NULL;

    zs.avail_in = 0;
    zs.next_in  = Z_NULL;

    if ( inflateInit(&zs) != Z_OK)
    {
        return -1;
    }

    zs.avail_in = in.size();
    zs.next_in  = (unsigned char *) const_cast<char *>(in.c_str());

    if ( zs.avail_in <= 2 ) //At least 2 byte header
    {
        inflateEnd(&zs);

        return -1;
    }

    unsigned char zbuf[ZBUFFER];
    int rc;

    do
    {
        zs.avail_out = ZBUFFER;
        zs.next_out  = zbuf;

        rc = inflate(&zs, Z_FINISH);

        if ( rc != Z_STREAM_END && rc != Z_OK && rc != Z_BUF_ERROR )
        {
            inflateEnd(&zs);

            return -1;
        }

        out.append((const char *) zbuf, (size_t) (ZBUFFER - zs.avail_out));
    } while (rc != Z_STREAM_END);

    inflateEnd(&zs);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */

int MonitorMessage::zlib_compress(const std::string& in, std::string& out)
{
    if ( in.empty() )
    {
        return -1;
    }

    z_stream zs;

    zs.zalloc = Z_NULL;
    zs.zfree  = Z_NULL;
    zs.opaque = Z_NULL;

    if ( deflateInit(&zs, Z_DEFAULT_COMPRESSION) != Z_OK )
    {
        return -1;
    }

    zs.avail_in = in.size();
    zs.next_in  = (unsigned char *) const_cast<char *>(in.c_str());

    unsigned char zbuf[ZBUFFER];

    do
    {
        zs.avail_out = ZBUFFER;
        zs.next_out  = zbuf;

        if ( deflate(&zs, Z_FINISH) == Z_STREAM_ERROR )
        {
            deflateEnd(&zs);
            return -1;
        }

        out.append((const char *) zbuf, (size_t) (ZBUFFER - zs.avail_out));
    } while (zs.avail_out == 0);

    deflateEnd(&zs);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
