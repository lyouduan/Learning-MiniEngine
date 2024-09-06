#pragma once
#include "GpuResource.h"
#include "Texture.h"
#include "GraphicsCommon.h"

// A referenced-counted pointer to a Texture.  See methods below.
class TextureRef;

//
// Texture file loading system
// 
// References to textures are passed around so that a texture may be shared. 
// When all references to a texture expire, the texture memory is reclaimed.
//
namespace TextureManager
{
	using Graphics::eDefaultTexture; // types of texture
	using Graphics::kMagenta2D; // Useful for indicating missing textures

	void Initialize(const std::wstring& RootPath);
	void Shutdown(void);

	// Load a texture from a DDS file.  Never returns null references, but if a 
    // texture cannot be found, ref->IsValid() will return false.
	TextureRef LoadDDSFromFile(const std::wstring& filePath, eDefaultTexture fallback = kMagenta2D, bool sRGB = false);
	TextureRef LoadDDSFromFile(const std::string& filePath, eDefaultTexture fallback = kMagenta2D, bool sRGB = false);
};

// Forward declaration; private implementation
class ManagedTexture;

//
// A handle to a ManagedTexture.  Constructors and destructors modify the reference
// count.  When the last reference is destroyed, the TextureManager is informed that
// the texture should be deleted.
//

class TextureRef
{
public:

	TextureRef(const TextureRef& ref);
	TextureRef(ManagedTexture* tex = nullptr);
	~TextureRef();

	void operator= (std::nullptr_t);
	void operator= (TextureRef& rhs);

	// Check that this points to a valid texture (which loaded successfully)
	bool IsValid() const;

	// Gets the SRV descriptor handle.  If the reference is invalid,
	// returns a valid descriptor handle (specified by the fallback)
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() const;

	// Get the texture pointer.  Client is responsible to not dereference
	// null pointers.
	const Texture* Get(void) const;

	const Texture* operator->(void) const;

private:
	ManagedTexture* m_ref;
};

