#ifndef GRAPH_LIB_H
#define GRAPH_LIB_H

namespace fallout {

unsigned char HighRGB(unsigned char color);
int load_lbm_to_buf(const char* path, unsigned char* dstBuffer, int xMin, int yMin, int xMax, int yMax);
int graphCompress(unsigned char* a1, unsigned char* a2, int a3);
int graphDecompress(const unsigned char* src, int srcSize, unsigned char* dest, int destSize);
void grayscalePaletteUpdate(int a1, int a2);
void grayscalePaletteApply(unsigned char* surface, int width, int height, int pitch);

} // namespace fallout

#endif /* GRAPH_LIB_H */
