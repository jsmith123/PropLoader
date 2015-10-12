#include <QFile>
#include <QTextStream>

#include "propellerloader.h"

#define LENGTH_FIELD_SIZE       11          /* number of bytes in the length field */

// Propeller Download Stream Translator array.  Index into this array using the "Binary Value" (usually 5 bits) to translate,
// the incoming bit size (again, usually 5), and the desired data element to retrieve (encoding = translation, bitCount = bit count
// actually translated.

// first index is the next 1-5 bits from the incoming bit stream
// second index is the number of bits in the first value
// the result is a structure containing the byte to output to encode some or all of the input bits
static struct {
    uint8_t encoding;   // encoded byte to output
    uint8_t bitCount;   // number of bits encoded by the output byte
} PDSTx[32][5] =

//  ***  1-BIT  ***        ***  2-BIT  ***        ***  3-BIT  ***        ***  4-BIT  ***        ***  5-BIT  ***
{ { /*%00000*/ {0xFE, 1},  /*%00000*/ {0xF2, 2},  /*%00000*/ {0x92, 3},  /*%00000*/ {0x92, 3},  /*%00000*/ {0x92, 3} },
  { /*%00001*/ {0xFF, 1},  /*%00001*/ {0xF9, 2},  /*%00001*/ {0xC9, 3},  /*%00001*/ {0xC9, 3},  /*%00001*/ {0xC9, 3} },
  {            {0,    0},  /*%00010*/ {0xFA, 2},  /*%00010*/ {0xCA, 3},  /*%00010*/ {0xCA, 3},  /*%00010*/ {0xCA, 3} },
  {            {0,    0},  /*%00011*/ {0xFD, 2},  /*%00011*/ {0xE5, 3},  /*%00011*/ {0x25, 4},  /*%00011*/ {0x25, 4} },
  {            {0,    0},             {0,    0},  /*%00100*/ {0xD2, 3},  /*%00100*/ {0xD2, 3},  /*%00100*/ {0xD2, 3} },
  {            {0,    0},             {0,    0},  /*%00101*/ {0xE9, 3},  /*%00101*/ {0x29, 4},  /*%00101*/ {0x29, 4} },
  {            {0,    0},             {0,    0},  /*%00110*/ {0xEA, 3},  /*%00110*/ {0x2A, 4},  /*%00110*/ {0x2A, 4} },
  {            {0,    0},             {0,    0},  /*%00111*/ {0xFA, 3},  /*%00111*/ {0x95, 4},  /*%00111*/ {0x95, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01000*/ {0x92, 3},  /*%01000*/ {0x92, 3} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01001*/ {0x49, 4},  /*%01001*/ {0x49, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01010*/ {0x4A, 4},  /*%01010*/ {0x4A, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01011*/ {0xA5, 4},  /*%01011*/ {0xA5, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01100*/ {0x52, 4},  /*%01100*/ {0x52, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01101*/ {0xA9, 4},  /*%01101*/ {0xA9, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01110*/ {0xAA, 4},  /*%01110*/ {0xAA, 4} },
  {            {0,    0},             {0,    0},             {0,    0},  /*%01111*/ {0xD5, 4},  /*%01111*/ {0xD5, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10000*/ {0x92, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10001*/ {0xC9, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10010*/ {0xCA, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10011*/ {0x25, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10100*/ {0xD2, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10101*/ {0x29, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10110*/ {0x2A, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%10111*/ {0x95, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11000*/ {0x92, 3} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11001*/ {0x49, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11010*/ {0x4A, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11011*/ {0xA5, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11100*/ {0x52, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11101*/ {0xA9, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11110*/ {0xAA, 4} },
  {            {0,    0},             {0,    0},             {0,    0},             {0,    0},  /*%11111*/ {0x55, 5} }
 };

// After reset, the Propeller's exact clock rate is not known by either the host or the Propeller itself, so communication
// with the Propeller takes place based on a host-transmitted timing template that the Propeller uses to read the stream
// and generate the responses.  The host first transmits the 2-bit timing template, then transmits a 250-bit Tx handshake,
// followed by 250 timing templates (one for each Rx handshake bit expected) which the Propeller uses to properly transmit
// the Rx handshake sequence.  Finally, the host transmits another eight timing templates (one for each bit of the
// Propeller's version number expected) which the Propeller uses to properly transmit it's 8-bit hardware/firmware version
// number.
//
// After the Tx Handshake and Rx Handshake are properly exchanged, the host and Propeller are considered "connected," at
// which point the host can send a download command followed by image size and image data, or simply end the communication.
//
// PROPELLER HANDSHAKE SEQUENCE: The handshake (both Tx and Rx) are based on a Linear Feedback Shift Register (LFSR) tap
// sequence that repeats only after 255 iterations.  The generating LFSR can be created in Pascal code as the following function
// (assuming FLFSR is pre-defined Byte variable that is set to ord('P') prior to the first call of IterateLFSR).  This is
// the exact function that was used in previous versions of the Propeller Tool and Propellent software.
//
//      function IterateLFSR: Byte;
//      begin //Iterate LFSR, return previous bit 0
//      Result := FLFSR and 0x01;
//      FLFSR := FLFSR shl 1 and 0xFE or (FLFSR shr 7 xor FLFSR shr 5 xor FLFSR shr 4 xor FLFSR shr 1) and 1;
//      end;
//
// The handshake bit stream consists of the lowest bit value of each 8-bit result of the LFSR described above.  This LFSR
// has a domain of 255 combinations, but the host only transmits the first 250 bits of the pattern, afterwards, the Propeller
// generates and transmits the next 250-bits based on continuing with the same LFSR sequence.  In this way, the host-
// transmitted (host-generated) stream ends 5 bits before the LFSR starts repeating the initial sequence, and the host-
// received (Propeller generated) stream that follows begins with those remaining 5 bits and ends with the leading 245 bits
// of the host-transmitted stream.
//
// For speed and compression reasons, this handshake stream has been encoded as tightly as possible into the pattern
// described below.
//
// The TxHandshake array consists of 209 bytes that are encoded to represent the required '1' and '0' timing template bits,
// 250 bits representing the lowest bit values of 250 iterations of the Propeller LFSR (seeded with ASCII 'P'), 250 more
// timing template bits to receive the Propeller's handshake response, and more to receive the version.
static uint8_t txHandshake[] = {
    // First timing template ('1' and '0') plus first two bits of handshake ('0' and '1').
    0x49,
    // Remaining 248 bits of handshake...
    0xAA,0x52,0xA5,0xAA,0x25,0xAA,0xD2,0xCA,0x52,0x25,0xD2,0xD2,0xD2,0xAA,0x49,0x92,
    0xC9,0x2A,0xA5,0x25,0x4A,0x49,0x49,0x2A,0x25,0x49,0xA5,0x4A,0xAA,0x2A,0xA9,0xCA,
    0xAA,0x55,0x52,0xAA,0xA9,0x29,0x92,0x92,0x29,0x25,0x2A,0xAA,0x92,0x92,0x55,0xCA,
    0x4A,0xCA,0xCA,0x92,0xCA,0x92,0x95,0x55,0xA9,0x92,0x2A,0xD2,0x52,0x92,0x52,0xCA,
    0xD2,0xCA,0x2A,0xFF,
    // 250 timing templates ('1' and '0') to receive 250-bit handshake from Propeller.
    // This is encoded as two pairs per byte; 125 bytes.
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,0x29,
    // 8 timing templates ('1' and '0') to receive 8-bit Propeller version; two pairs per byte; 4 bytes.
    0x29,0x29,0x29,0x29};

// Shutdown command (0); 11 bytes.
static uint8_t shutdownCmd[] = {0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xf2};

// Load RAM and Run command (1); 11 bytes.
static uint8_t loadRunCmd[] = {0xc9, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xf2};

// Load RAM, Program EEPROM, and Shutdown command (2); 11 bytes.
static uint8_t programShutdownCmd[] = {0xca, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xf2};

// Load RAM, Program EEPROM, and Run command (3); 11 bytes.
static uint8_t programRunCmd[] = {0x25, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0x92, 0xfe};

// The RxHandshake array consists of 125 bytes encoded to represent the expected 250-bit (125-byte @ 2 bits/byte) response
// of continuing-LFSR stream bits from the Propeller, prompted by the timing templates following the TxHandshake stream.
static uint8_t rxHandshake[] = {
    0xEE,0xCE,0xCE,0xCF,0xEF,0xCF,0xEE,0xEF,0xCF,0xCF,0xEF,0xEF,0xCF,0xCE,0xEF,0xCF,
    0xEE,0xEE,0xCE,0xEE,0xEF,0xCF,0xCE,0xEE,0xCE,0xCF,0xEE,0xEE,0xEF,0xCF,0xEE,0xCE,
    0xEE,0xCE,0xEE,0xCF,0xEF,0xEE,0xEF,0xCE,0xEE,0xEE,0xCF,0xEE,0xCF,0xEE,0xEE,0xCF,
    0xEF,0xCE,0xCF,0xEE,0xEF,0xEE,0xEE,0xEE,0xEE,0xEF,0xEE,0xCF,0xCF,0xEF,0xEE,0xCE,
    0xEF,0xEF,0xEF,0xEF,0xCE,0xEF,0xEE,0xEF,0xCF,0xEF,0xCF,0xCF,0xCE,0xCE,0xCE,0xCF,
    0xCF,0xEF,0xCE,0xEE,0xCF,0xEE,0xEF,0xCE,0xCE,0xCE,0xEF,0xEF,0xCF,0xCF,0xEE,0xEE,
    0xEE,0xCE,0xCF,0xCE,0xCE,0xCF,0xCE,0xEE,0xEF,0xEE,0xEF,0xEF,0xCF,0xEF,0xCE,0xCE,
    0xEF,0xCE,0xEE,0xCE,0xEF,0xCE,0xCE,0xEE,0xCF,0xCF,0xCE,0xCF,0xCF};

PropellerLoader::PropellerLoader(PropellerConnection &connection) : m_connection(connection)
{
}

PropellerLoader::~PropellerLoader()
{
}

#if 0
int PropellerLoader::load(const char *fileName)
{
    QFile file(fileName);
    file.open(QIODevice::ReadOnly);

    int imageSize = (int)file.size();
    uint8_t *image = new uint8_t [imageSize];
    file.read((char *)image, imageSize);
    file.close();

    int err = load(image, imageSize);

    delete image;

    return err;
}
#endif

int PropellerLoader::load(const char *file, LoadType loadType)
{
    int imageSize, sts;
    uint8_t *image;
    ElfHdr elfHdr;
    FILE *fp;

    /* open the binary */
    if (!(fp = fopen(file, "rb"))) {
        printf("error: can't open '%s'\n", file);
        return -1;
    }

    /* check for an elf file */
    if (ReadAndCheckElfHdr(fp, &elfHdr)) {
        image = loadElfFile(fp, &elfHdr, &imageSize);
        fclose(fp);
    }
    else {
        image = loadSpinBinaryFile(fp, &imageSize);
        fclose(fp);
    }

    /* make sure the image was loaded into memory */
    if (!image)
        return -1;

    /* load the file */
    if ((sts = load(image, imageSize, loadType)) != 0) {
        free(image);
        return -1;
    }

    /* return successfully */
    free(image);
    return 0;
}

/* target checksum for a binary file */
#define SPIN_TARGET_CHECKSUM    0x14

uint8_t *PropellerLoader::loadSpinBinaryFile(FILE *fp, int *pLength)
{
    uint8_t *image;
    int imageSize;

    /* get the size of the binary file */
    fseek(fp, 0, SEEK_END);
    imageSize = (int)ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* allocate space for the file */
    if (!(image = (uint8_t *)malloc(imageSize)))
        return NULL;

    /* read the entire image into memory */
    if ((int)fread(image, 1, imageSize, fp) != imageSize) {
        free(image);
        return NULL;
    }

    /* return the buffer containing the file contents */
    *pLength = imageSize;
    return image;
}

/* spin object file header */
typedef struct {
    uint32_t clkfreq;
    uint8_t clkmode;
    uint8_t chksum;
    uint16_t pbase;
    uint16_t vbase;
    uint16_t dbase;
    uint16_t pcurr;
    uint16_t dcurr;
} SpinHdr;

uint8_t *PropellerLoader::loadElfFile(FILE *fp, ElfHdr *hdr, int *pImageSize)
{
    uint32_t start, imageSize, cogImagesSize;
    uint8_t *image, *buf, *p;
    ElfProgramHdr program;
    int chksum, cnt, i;
    SpinHdr *spinHdr;
    ElfContext *c;

    /* open the elf file */
    if (!(c = OpenElfFile(fp, hdr)))
        return NULL;

    /* get the total size of the program */
    if (!GetProgramSize(c, &start, &imageSize, &cogImagesSize))
        goto fail;

    /* cog images in eeprom are not allowed */
    if (cogImagesSize > 0)
        goto fail;

    /* allocate a buffer big enough for the entire image */
    if (!(image = (uint8_t *)malloc(imageSize)))
        goto fail;
    memset(image, 0, imageSize);

    /* load each program section */
    for (i = 0; i < c->hdr.phnum; ++i) {
        if (!LoadProgramTableEntry(c, i, &program)
        ||  !(buf = LoadProgramSegment(c, &program))) {
            free(image);
            goto fail;
        }
        if (program.paddr < COG_DRIVER_IMAGE_BASE)
            memcpy(&image[program.paddr - start], buf, program.filesz);
    }

    /* free the elf file context */
    FreeElfContext(c);

    /* fixup the spin binary header */
    spinHdr = (SpinHdr *)image;
    spinHdr->vbase = imageSize;
    spinHdr->dbase = imageSize + 2 * sizeof(uint32_t); // stack markers
    spinHdr->dcurr = spinHdr->dbase + sizeof(uint32_t);

    /* update the checksum */
    spinHdr->chksum = chksum = 0;
    p = image;
    for (cnt = imageSize; --cnt >= 0; )
        chksum += *p++;
    spinHdr->chksum = SPIN_TARGET_CHECKSUM - chksum;

    /* return the image */
    *pImageSize = imageSize;
    return image;

fail:
    /* return failure */
    FreeElfContext(c);
    return NULL;
}

int PropellerLoader::load(uint8_t *image, int size, LoadType loadType)
{
    QByteArray packet;
    QByteArray verificationPacket(1024, 0xF9);
    uint8_t buf[sizeof(rxHandshake) + 4];
    int version, cnt, i;

    /* generate a single packet containing the tx handshake and the image to load */
    generateLoaderPacket(packet, image, size);

    /* reset the Propeller */
    m_connection.generateResetSignal();

    /* send the packet */
    //printf("Send second-stage loader image\n");
    m_connection.sendData((uint8_t *)packet.data(), packet.size());

    /* pause while the transfer occurs */
    m_connection.pauseForVerification(packet.size());

    /* send the verification packet (all timing templates) */
    //printf("Send verification packet\n");
//    m_connection.sendData((uint8_t *)verificationPacket.data(), verificationPacket.size());

    /* receive the handshake response and the hardware version */
    //printf("Receive handshake response\n");
    cnt = m_connection.receiveDataExactTimeout(buf, sizeof(rxHandshake) + 4, 2000);

    /* verify the rx handshake */
    if (cnt != sizeof(rxHandshake) + 4 || memcmp(buf, rxHandshake, sizeof(rxHandshake)) != 0) {
        printf("error: handshake failed\n");
        return -1;
    }

    /* verify the hardware version */
    version = 0;
    for (i = sizeof(rxHandshake); i < cnt; ++i)
        version = ((version >> 2) & 0x3F) | ((buf[i] & 0x01) << 6) | ((buf[i] & 0x20) << 2);
    if (version != 1) {
        printf("error: wrong propeller version\n");
        return -1;
    }

    /* receive and verify the checksum */
    //printf("Receive checksum\n");
    if (m_connection.receiveChecksumAck(packet.size()) != 0)
        return -1;
#if 0
    cnt = m_connection.receiveDataExactTimeout(buf, 1, 2000);
    if (cnt != 1 || buf[0] != 0xFE) {
        printf("error: loader checksum failed\n");
        return -1;
    }
#endif

    /* return successfully */
    //printf("Load completed\n");
    return 0;
}

void PropellerLoader::generateIdentifyPacket(QByteArray &packet)
{
    /* initialize the packet */
    packet.clear();

    /* copy the handshake image and the shutdown command to the packet */
    packet.append((char *)txHandshake, sizeof(txHandshake));
    packet.append((char *)shutdownCmd, sizeof(shutdownCmd));
}

void PropellerLoader::generateLoaderPacket(QByteArray &packet, const uint8_t *image, int imageSize)
{
    int imageSizeInLongs = (imageSize + 3) / 4;
    int tmp, i;

    /* initialize the packet */
    packet.clear();

    /* copy the handshake image and the command to the packet */
    packet.append((char *)txHandshake, sizeof(txHandshake));
    packet.append((char *)loadRunCmd, sizeof(loadRunCmd));

    /* add the image size in longs */
    tmp = imageSizeInLongs;
    for (i = 0; i < LENGTH_FIELD_SIZE; ++i) {
        packet.append(0x92 | (i == 10 ? 0x60 : 0x00) | (tmp & 1) | ((tmp & 2) << 2) | ((tmp & 4) << 4));
        tmp >>= 3;
    }

    /* encode the image and insert it into the packet */
    encodeBytes(packet, image, imageSize);
}

/* encodeBytes
    parameters:
        packet is a packet to receive the encoded bytes
        inBytes is a pointer to a buffer of bytes to be encoded
        inCount is the number of bytes in inBytes
    returns the number of bytes written to the outBytes buffer or -1 if the encoded data does not fit
*/
void PropellerLoader::encodeBytes(QByteArray &packet, const uint8_t *inBytes, int inCount)
{
    static uint8_t masks[] = { 0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f };
    int bitCount = inCount * 8;
    int nextBit = 0;

    /* encode all bits in the input buffer */
    while (nextBit < bitCount) {
        int bits, bitsIn;

        /* encode 5 bits or whatever remains in inBytes, whichever is smaller */
        bitsIn = bitCount - nextBit;
        if (bitsIn > 5)
            bitsIn = 5;

        /* extract the next 'bitsIn' bits from the input buffer */
        bits = ((inBytes[nextBit / 8] >> (nextBit % 8)) | (inBytes[nextBit / 8 + 1] << (8 - (nextBit % 8)))) & masks[bitsIn];

        /* store the encoded value */
        packet.append(PDSTx[bits][bitsIn - 1].encoding);

        /* advance to the next group of bits */
        nextBit += PDSTx[bits][bitsIn - 1].bitCount;
    }
}
