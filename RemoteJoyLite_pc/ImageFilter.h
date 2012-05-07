#ifndef IMAGE_FILTER_H
#define IMAGE_FILTER_H

struct IDirect3DDevice9;

class ImageFilter {
public:
	ImageFilter() { }
	virtual ~ImageFilter() { }
	virtual void set(IDirect3DDevice9* device) = 0;
};

#endif
