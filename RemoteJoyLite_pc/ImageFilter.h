#ifndef IMAGE_FILTER_H
#define IMAGE_FILTER_H

struct IDirect3DDevice9;

class ImageFilter {
public:
	ImageFilter(IDirect3DDevice9* device) : device(device) { }
	virtual ~ImageFilter() { }
	virtual void set() const = 0;

protected:
	IDirect3DDevice9* device;
};

#endif
