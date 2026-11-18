// Copyright (c) 2014-2026 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_HASH_PHR_H
#define BITCOIN_HASH_PHR_H

#include <uint256.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace phr_pow {

using Hash = std::array<std::uint8_t, 32>;

inline std::uint32_t Rotr32(std::uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
inline std::uint64_t Rotl64(std::uint64_t x, int n) { return (x << n) | (x >> (64 - n)); }
inline std::uint32_t Load32LE(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
inline std::uint32_t Load32BE(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}
inline std::uint64_t Load64LE(const std::uint8_t* p) {
    std::uint64_t value = 0;
    for (int i = 0; i < 8; ++i) value |= static_cast<std::uint64_t>(p[i]) << (8 * i);
    return value;
}
inline void Store32LE(std::uint8_t* p, std::uint32_t value) {
    p[0] = static_cast<std::uint8_t>(value); p[1] = static_cast<std::uint8_t>(value >> 8);
    p[2] = static_cast<std::uint8_t>(value >> 16); p[3] = static_cast<std::uint8_t>(value >> 24);
}
inline void Store32BE(std::uint8_t* p, std::uint32_t value) {
    p[0] = static_cast<std::uint8_t>(value >> 24); p[1] = static_cast<std::uint8_t>(value >> 16);
    p[2] = static_cast<std::uint8_t>(value >> 8); p[3] = static_cast<std::uint8_t>(value);
}
inline void Store64LE(std::uint8_t* p, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>(value >> (8 * i));
}
inline void Store64BE(std::uint8_t* p, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) p[i] = static_cast<std::uint8_t>(value >> (56 - 8 * i));
}

inline void Sha256Transform(std::uint32_t state[8], const std::uint8_t block[64]) {
    static const std::uint32_t K[64] = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U};
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) w[i] = Load32BE(block + 4 * i);
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = Rotr32(w[i - 15], 7) ^ Rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = Rotr32(w[i - 2], 17) ^ Rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t s1 = Rotr32(e,6)^Rotr32(e,11)^Rotr32(e,25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t temp1 = h + s1 + ch + K[i] + w[i];
        const std::uint32_t s0 = Rotr32(a,2)^Rotr32(a,13)^Rotr32(a,22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = s0 + maj;
        h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d; state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}

inline Hash Sha256(const std::uint8_t* data, std::size_t len) {
    std::uint32_t state[8] = {0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    const std::uint8_t* cursor = data; std::size_t remaining = len;
    while (remaining >= 64) { Sha256Transform(state, cursor); cursor += 64; remaining -= 64; }
    std::uint8_t block[64]; std::memset(block, 0, sizeof(block));
    if (remaining > 0) std::memcpy(block, cursor, remaining);
    block[remaining] = 0x80;
    if (remaining >= 56) { Sha256Transform(state, block); std::memset(block, 0, sizeof(block)); }
    Store64BE(block + 56, static_cast<std::uint64_t>(len) * 8);
    Sha256Transform(state, block);
    Hash out{}; for (int i = 0; i < 8; ++i) Store32BE(out.data() + 4 * i, state[i]);
    return out;
}

inline void KeccakF1600(std::uint64_t st[25]) {
    static const std::uint64_t RC[24] = {
        0x0000000000000001ULL,0x0000000000008082ULL,0x800000000000808aULL,0x8000000080008000ULL,
        0x000000000000808bULL,0x0000000080000001ULL,0x8000000080008081ULL,0x8000000000008009ULL,
        0x000000000000008aULL,0x0000000000000088ULL,0x0000000080008009ULL,0x000000008000000aULL,
        0x000000008000808bULL,0x800000000000008bULL,0x8000000000008089ULL,0x8000000000008003ULL,
        0x8000000000008002ULL,0x8000000000000080ULL,0x000000000000800aULL,0x800000008000000aULL,
        0x8000000080008081ULL,0x8000000000008080ULL,0x0000000080000001ULL,0x8000000080008008ULL};
    static const int kRotc[24] = {1,3,6,10,15,21,28,36,45,55,2,14,27,41,56,8,25,43,62,18,39,61,20,44};
    static const int kPiln[24] = {10,7,11,17,18,3,5,16,8,21,24,4,15,23,19,13,12,2,20,14,22,9,6,1};
    for (int round = 0; round < 24; ++round) {
        std::uint64_t bc[5];
        for (int i = 0; i < 5; ++i) bc[i] = st[i]^st[i+5]^st[i+10]^st[i+15]^st[i+20];
        for (int i = 0; i < 5; ++i) { const std::uint64_t t = bc[(i+4)%5]^Rotl64(bc[(i+1)%5],1); for(int j=0;j<25;j+=5) st[j+i]^=t; }
        std::uint64_t t = st[1];
        for (int i = 0; i < 24; ++i) { int j=kPiln[i]; std::uint64_t saved=st[j]; st[j]=Rotl64(t,kRotc[i]); t=saved; }
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; ++i) bc[i]=st[j+i];
            for (int i = 0; i < 5; ++i) st[j+i]^=(~bc[(i+1)%5])&bc[(i+2)%5];
        }
        st[0]^=RC[round];
    }
}

inline Hash Sha3_256(const std::uint8_t* data, std::size_t len) {
    const std::size_t kRate = 136; std::uint64_t st[25]; std::memset(st,0,sizeof(st));
    const std::uint8_t* cursor=data; std::size_t remaining=len;
    while (remaining >= kRate) { for (std::size_t i=0;i<kRate/8;++i) st[i]^=Load64LE(cursor+8*i); KeccakF1600(st); cursor+=kRate; remaining-=kRate; }
    std::uint8_t block[kRate]; std::memset(block,0,sizeof(block));
    if (remaining>0) std::memcpy(block,cursor,remaining);
    block[remaining]=0x06; block[kRate-1]|=0x80;
    for (std::size_t i=0;i<kRate/8;++i) st[i]^=Load64LE(block+8*i); KeccakF1600(st);
    Hash out{}; for (int i=0;i<4;++i) Store64LE(out.data()+8*i, st[i]); return out;
}

inline void Blake2sCompress(std::uint32_t h[8], const std::uint8_t block[64], std::uint64_t counter, bool last) {
    static const std::uint32_t IV[8] = {0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    static const std::uint8_t kSigma[10][16] = {
        {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15},{14,10,4,8,9,15,13,6,1,12,0,2,11,7,5,3},
        {11,8,12,0,5,2,15,13,10,14,3,6,7,1,9,4},{7,9,3,1,13,12,11,14,2,6,5,10,4,0,15,8},
        {9,0,5,7,2,4,10,15,14,1,11,12,6,8,3,13},{2,12,6,10,0,11,8,3,4,13,7,5,15,14,1,9},
        {12,5,1,15,14,13,4,10,0,7,6,3,9,2,8,11},{13,11,7,14,12,1,3,9,5,0,15,4,8,6,2,10},
        {6,15,14,9,11,3,0,8,12,2,13,7,1,4,10,5},{10,2,8,4,7,6,1,5,15,11,9,14,3,12,13,0}};
    std::uint32_t m[16], v[16];
    for (int i=0;i<16;++i) m[i]=Load32LE(block+4*i);
    for (int i=0;i<8;++i){ v[i]=h[i]; v[i+8]=IV[i]; }
    v[12]^=static_cast<std::uint32_t>(counter); v[13]^=static_cast<std::uint32_t>(counter>>32);
    if (last) v[14]=~v[14];
#define PHR_BLAKE2S_G(a,b,c,d,x,y) do{ v[a]=v[a]+v[b]+(x); v[d]=Rotr32(v[d]^v[a],16); v[c]=v[c]+v[d]; v[b]=Rotr32(v[b]^v[c],12); v[a]=v[a]+v[b]+(y); v[d]=Rotr32(v[d]^v[a],8); v[c]=v[c]+v[d]; v[b]=Rotr32(v[b]^v[c],7);}while(0)
    for (int r=0;r<10;++r){
        PHR_BLAKE2S_G(0,4,8,12,m[kSigma[r][0]],m[kSigma[r][1]]);
        PHR_BLAKE2S_G(1,5,9,13,m[kSigma[r][2]],m[kSigma[r][3]]);
        PHR_BLAKE2S_G(2,6,10,14,m[kSigma[r][4]],m[kSigma[r][5]]);
        PHR_BLAKE2S_G(3,7,11,15,m[kSigma[r][6]],m[kSigma[r][7]]);
        PHR_BLAKE2S_G(0,5,10,15,m[kSigma[r][8]],m[kSigma[r][9]]);
        PHR_BLAKE2S_G(1,6,11,12,m[kSigma[r][10]],m[kSigma[r][11]]);
        PHR_BLAKE2S_G(2,7,8,13,m[kSigma[r][12]],m[kSigma[r][13]]);
        PHR_BLAKE2S_G(3,4,9,14,m[kSigma[r][14]],m[kSigma[r][15]]);
    }
#undef PHR_BLAKE2S_G
    for (int i=0;i<8;++i) h[i]^=v[i]^v[i+8];
}
inline Hash Blake2s(const std::uint8_t* data, std::size_t len) {
    std::uint32_t h[8]={0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    h[0]^=0x01010000U^32U;
    const std::uint8_t* cursor=data; std::size_t remaining=len; std::uint64_t counter=0;
    while (remaining>64){ counter+=64; Blake2sCompress(h,cursor,counter,false); cursor+=64; remaining-=64; }
    std::uint8_t block[64]; std::memset(block,0,sizeof(block)); if(remaining>0) std::memcpy(block,cursor,remaining);
    counter+=remaining; Blake2sCompress(h,block,counter,true);
    Hash out{}; for(int i=0;i<8;++i) Store32LE(out.data()+4*i,h[i]); return out;
}

inline std::uint32_t Rotl32c(std::uint32_t x,int n){ return (x<<n)|(x>>(32-n)); }
inline void ChaCha20Block(const Hash& key, std::uint64_t counter, std::uint8_t out[64]) {
    std::uint32_t state[16];
    state[0]=0x61707865U; state[1]=0x3320646eU; state[2]=0x79622d32U; state[3]=0x6b206574U;
    for(int i=0;i<8;++i) state[4+i]=Load32LE(key.data()+4*i);
    state[12]=static_cast<std::uint32_t>(counter); state[13]=static_cast<std::uint32_t>(counter>>32); state[14]=0; state[15]=0;
    std::uint32_t x[16]; for(int i=0;i<16;++i) x[i]=state[i];
#define PHR_CHACHA_QR(a,b,c,d) do{ x[a]+=x[b]; x[d]=Rotl32c(x[d]^x[a],16); x[c]+=x[d]; x[b]=Rotl32c(x[b]^x[c],12); x[a]+=x[b]; x[d]=Rotl32c(x[d]^x[a],8); x[c]+=x[d]; x[b]=Rotl32c(x[b]^x[c],7);}while(0)
    for(int i=0;i<10;++i){
        PHR_CHACHA_QR(0,4,8,12); PHR_CHACHA_QR(1,5,9,13); PHR_CHACHA_QR(2,6,10,14); PHR_CHACHA_QR(3,7,11,15);
        PHR_CHACHA_QR(0,5,10,15); PHR_CHACHA_QR(1,6,11,12); PHR_CHACHA_QR(2,7,8,13); PHR_CHACHA_QR(3,4,9,14);
    }
#undef PHR_CHACHA_QR
    for(int i=0;i<16;++i) Store32LE(out+4*i, x[i]+state[i]);
}

inline void Append(std::vector<std::uint8_t>& out, const std::uint8_t* data, std::size_t len){ out.insert(out.end(), data, data+len); }
inline void Append(std::vector<std::uint8_t>& out, const Hash& hash){ out.insert(out.end(), hash.begin(), hash.end()); }
inline std::vector<std::uint8_t> TaggedInput(const char* tag){ std::vector<std::uint8_t> in(tag, tag+std::strlen(tag)); in.push_back(0); return in; }

inline void MakeDataset(const Hash& key, std::size_t bytes, std::vector<std::uint8_t>& out) {
    out.resize(bytes);
    Hash chain = key; std::size_t pos = 0;
    while (pos < bytes) {
        std::uint8_t block[64]; ChaCha20Block(chain, 0, block);
        const std::size_t chunk = (bytes-pos < sizeof(block)) ? (bytes-pos) : sizeof(block);
        std::memcpy(out.data()+pos, block, chunk);
        std::memcpy(chain.data(), block, 32);
        pos += chunk;
    }
}

inline Hash HashHeaderPHR(const std::uint8_t* header, std::size_t header_len,
                          const char* pers_c = "PHR",
                          std::size_t dataset_bytes = 1024*1024, std::uint32_t rounds = 32) {
    const std::uint8_t* pers = reinterpret_cast<const std::uint8_t*>(pers_c);
    const std::size_t pers_len = std::strlen(pers_c);
    if (dataset_bytes < 64 || dataset_bytes % 64 != 0) throw std::runtime_error("dataset_bytes must be >= 64 and divisible by 64");
    if (rounds == 0) throw std::runtime_error("rounds must be > 0");
    auto dataset_input = TaggedInput("PHR-YP-DATASET");
    Append(dataset_input, header, header_len); Append(dataset_input, pers, pers_len);
    const Hash dataset_key = Sha256(dataset_input.data(), dataset_input.size());
    static thread_local std::vector<std::uint8_t> dataset;
    MakeDataset(dataset_key, dataset_bytes, dataset);
    auto init_input = TaggedInput("PHR-YP-INIT");
    Append(init_input, header, header_len); Append(init_input, pers, pers_len);
    Hash state = Sha256(init_input.data(), init_input.size());
    const std::uint64_t lines = dataset_bytes / 64;
    for (std::uint32_t r = 0; r < rounds; ++r) {
        const std::uint64_t idx = Load64LE(state.data()) % lines;
        const std::uint8_t* line = dataset.data() + idx * 64;
        const std::uint8_t selector = state[8] % 3;
        std::uint8_t msg[32 + 64 + 4 + 1];
        std::memcpy(msg, state.data(), 32);
        std::memcpy(msg + 32, line, 64);
        Store32LE(msg + 96, r);
        msg[100] = selector;
        if (selector == 0) state = Sha256(msg, sizeof(msg));
        else if (selector == 1) state = Sha3_256(msg, sizeof(msg));
        else state = Blake2s(msg, sizeof(msg));
    }
    auto final_input = TaggedInput("PHR-YP-FINAL");
    Append(final_input, state); Append(final_input, header, header_len); Append(final_input, pers, pers_len);
    return Sha256(final_input.data(), final_input.size());
}

} // namespace phr_pow

/** PoW hash (PHR-256-YP). Returns uint256 with numeric value equal to the
 *  big-endian PHR digest (so GetHex() matches the digest). */
template <typename T1>
inline uint256 HashPHR(const T1 pbegin, const T1 pend)
{
    const auto* p = (pbegin == pend) ? static_cast<const std::uint8_t*>(nullptr)
                                     : reinterpret_cast<const std::uint8_t*>(&pbegin[0]);
    const std::size_t len = static_cast<std::size_t>(pend - pbegin);
    auto digest = phr_pow::HashHeaderPHR(p ? p : reinterpret_cast<const std::uint8_t*>(""), len, "PHR");
    uint256 result;
    for (std::size_t i = 0; i < 32; ++i) {
        *(result.begin() + i) = digest[31 - i];
    }
    return result;
}


#endif // BITCOIN_HASH_PHR_H
