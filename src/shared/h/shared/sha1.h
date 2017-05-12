#ifndef HEADER_282EFAD0D03545DA82A69007122DD720 
#define HEADER_282EFAD0D03545DA82A69007122DD720

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class SHA1 {
public:
    static const size_t BLOCK_SIZE=64;
    static const size_t DIGEST_SIZE=20;
    static const size_t DIGEST_STR_LEN=40;
    static const size_t DIGEST_STR_SIZE=DIGEST_STR_LEN+1;

    SHA1();

    void Update(const void *data,size_t data_size);
    void Finish(uint8_t digest[DIGEST_SIZE],char digest_str[DIGEST_STR_SIZE]);

    static void HashBuffer(uint8_t digest[DIGEST_SIZE],char digest_str[DIGEST_STR_SIZE],const void *data,size_t data_size);
protected:
private:
    uint32_t m_state[5];
    uint64_t m_count;
    unsigned char m_buffer[BLOCK_SIZE];

    void Pad();
    void Final(uint8_t digest[DIGEST_SIZE]);
    void Transform(uint32_t state[5],const uint8_t buffer[BLOCK_SIZE]);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif//HEADER_282EFAD0D03545DA82A69007122DD720
