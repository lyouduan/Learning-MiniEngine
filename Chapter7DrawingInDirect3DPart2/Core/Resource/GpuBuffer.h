#pragma once

#include "pch.h"
#include "GpuResource.h"

class UploadBuffer;

class GpuBuffer : public GpuResource
{
public:

	virtual ~GpuBuffer() { Destroy(); };

	// create a buffer.
	// if initial data is provided, it will be copied into the buffer using the default command context
	void Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize,
		const void* initialData = nullptr);

	// explicitly use upload buffer, so that it don't apply a upload buffer
	void Create(const std::wstring& name, uint32_t NumElements, uint32_t ElementSize,
		const UploadBuffer& srcData, uint32_t srcOffset = 0);

	// Sub-Allocate a buffer out of a pre-allocated heap.  If initial data is provided, it will be copied into the buffer using the default command context.
	void CreatePlaced(const std::wstring& name, ID3D12Heap* pBackingHeap, uint32_t HeapOffset, uint32_t NumElements, uint32_t ElementSize,
		const void* initialData = nullptr);

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetUAV(void) const { return m_UAV; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const { return m_SRV; }

	D3D12_GPU_VIRTUAL_ADDRESS RootConstantBufferView(void) const { return m_GpuVirtualAddress; }

	D3D12_CPU_DESCRIPTOR_HANDLE CreateConstantBufferView(uint32_t Offset, uint32_t Size) const;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t Offset, uint32_t Size, uint32_t Stride) const;
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView(size_t BaseVertexIndex = 0) const
	{
		size_t Offset = BaseVertexIndex * m_ElementSize;
		return VertexBufferView(Offset, (uint32_t)(m_BufferSize - Offset), m_ElementSize);
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t Offset, uint32_t Size, bool b32Bit = false) const;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView(size_t StartIndex = 0) const
	{
		size_t Offset = StartIndex * m_ElementSize;
		return IndexBufferView(Offset, (uint32_t)(m_BufferSize - Offset), m_ElementSize == 4);
	}

	size_t GetBufferSize() const { return m_BufferSize; }
	uint32_t GetElementCount() const { return m_ElementCount; }
	uint32_t GetElementSize() const { return m_ElementSize; }

protected:

	GpuBuffer(void) : m_BufferSize(0), m_ElementCount(0), m_ElementSize(0)
	{
		m_ResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		m_UAV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_SRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	D3D12_RESOURCE_DESC DescribeBuffer(void);
	virtual void CreateDerivedViews(void) = 0; // pure virtual 

	D3D12_CPU_DESCRIPTOR_HANDLE m_UAV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_SRV;

	size_t m_BufferSize;
	uint32_t m_ElementCount;
	uint32_t m_ElementSize;
	D3D12_RESOURCE_FLAGS m_ResourceFlags;
};

//
//  derived class
// 

//字节地址缓冲区
class ByteAddressBuffer : public GpuBuffer
{
public:
	virtual void CreateDerivedViews(void) override;
};

//用于表示间接参数缓冲区。这种缓冲区通常用于间接绘制或间接计算调度。
class IndirectArgsBuffer : public ByteAddressBuffer
{
public:
	IndirectArgsBuffer(void)
	{
	}
};

// 结构化缓冲区（每个元素的大小相同，通常用于存储结构体数组
class StructuredBuffer : public GpuBuffer
{
public:
	virtual void Destroy(void) override
	{
		m_CounterBuffer.Destroy();
		GpuBuffer::Destroy();
	}

	virtual void CreateDerivedViews(void) override;

	ByteAddressBuffer& GetCounterBuffer(void) { return m_CounterBuffer; }

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetCounterSRV(CommandContext& Context);
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetCounterUAV(CommandContext& Context);

private:
	ByteAddressBuffer m_CounterBuffer;
};

// 具有固定数据类型的缓冲区
class TypedBuffer : public GpuBuffer
{
public:
	TypedBuffer(DXGI_FORMAT Format) : m_DataFormat(Format) {}
	virtual void CreateDerivedViews(void) override;

protected:
	DXGI_FORMAT m_DataFormat;
};
