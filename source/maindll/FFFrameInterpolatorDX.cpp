#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include "NGX/NvNGX.h"
#include "FFFrameInterpolatorDX.h"

D3D12_RESOURCE_STATES ffxGetDX12StateFromResourceState(FfxResourceStates state);

FFFrameInterpolatorDX::FFFrameInterpolatorDX(
	ID3D12Device *Device,
	uint32_t OutputWidth,
	uint32_t OutputHeight,
	NGXInstanceParameters *NGXParameters)
	: m_Device(Device),
	  FFFrameInterpolator(OutputWidth, OutputHeight)
{
	FFFrameInterpolator::Create(NGXParameters);
	m_Device->AddRef();
}

FFFrameInterpolatorDX::~FFFrameInterpolatorDX()
{
	FFFrameInterpolator::Destroy();
	m_Device->Release();
}

FfxErrorCode FFFrameInterpolatorDX::Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters)
{
	const bool isRecordingCommands = NGXParameters->GetUIntOrDefault("DLSSG.IsRecording", 0) != 0;
	const auto cmdList12 = reinterpret_cast<ID3D12GraphicsCommandList *>(CommandList);

	NGXParameters->Set4("DLSSG.FlushRequired", 0);

	// Begin a new command list in the event our caller didn't set one up
	if (!isRecordingCommands)
	{
		ID3D12CommandQueue *recordingQueue = nullptr;
		NGXParameters->GetVoidPointer("DLSSG.CmdQueue", reinterpret_cast<void **>(&recordingQueue));

		ID3D12CommandAllocator *recordingAllocator = nullptr;
		NGXParameters->GetVoidPointer("DLSSG.CmdAlloc", reinterpret_cast<void **>(&recordingAllocator));

		cmdList12->Reset(recordingAllocator, nullptr);
	}

	m_ActiveCommandList = ffxGetCommandListDX12(cmdList12);
	const auto interpolationResult = FFFrameInterpolator::Dispatch(nullptr, NGXParameters);

	// Finish what we started. Restore the command list to its previous state when necessary.
	if (!isRecordingCommands)
		cmdList12->Close();

	return interpolationResult;
}

FfxErrorCode FFFrameInterpolatorDX::InitializeBackendInterface(
	FFInterfaceWrapper *BackendInterface,
	uint32_t MaxContexts,
	NGXInstanceParameters *NGXParameters)
{
	return BackendInterface->Initialize(m_Device, MaxContexts, NGXParameters);
}

FfxCommandList FFFrameInterpolatorDX::GetActiveCommandList() const
{
	return m_ActiveCommandList;
}

std::array<uint8_t, 8> FFFrameInterpolatorDX::GetActiveAdapterLUID() const
{
	const auto luid = m_Device->GetAdapterLuid();

	std::array<uint8_t, sizeof(luid)> result;
	memcpy(result.data(), &luid, result.size());

	return result;
}

void FFFrameInterpolatorDX::CopyTexture(FfxCommandList CommandList, const FfxResource *Destination, const FfxResource *Source)
{
	if (Destination->resource == Source->resource)
		return;

	const auto cmdList12 = reinterpret_cast<ID3D12GraphicsCommandList *>(CommandList);

	D3D12_RESOURCE_BARRIER barriers[2] = {};
	uint32_t barrierCount = 0;

	const auto destBeforeState = ffxGetDX12StateFromResourceState(Destination->state);
	const auto destAfterState = D3D12_RESOURCE_STATE_COPY_DEST;

	if (destBeforeState != destAfterState)
	{
		auto& barrier = barriers[barrierCount++];
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = static_cast<ID3D12Resource *>(Destination->resource);
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = destBeforeState;
		barrier.Transition.StateAfter = destAfterState;
	}

	const auto srcBeforeState = ffxGetDX12StateFromResourceState(Source->state);
	const auto srcAfterState = D3D12_RESOURCE_STATE_COPY_SOURCE;

	if (srcBeforeState != srcAfterState)
	{
		auto& barrier = barriers[barrierCount++];
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = static_cast<ID3D12Resource *>(Source->resource);
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Transition.StateBefore = srcBeforeState;
		barrier.Transition.StateAfter = srcAfterState;
	}

	if (barrierCount > 0)
		cmdList12->ResourceBarrier(barrierCount, barriers);

	cmdList12->CopyResource(static_cast<ID3D12Resource *>(Destination->resource), static_cast<ID3D12Resource *>(Source->resource));

	if (barrierCount > 0)
	{
		for (uint32_t i = 0; i < barrierCount; i++)
			std::swap(barriers[i].Transition.StateBefore, barriers[i].Transition.StateAfter);

		cmdList12->ResourceBarrier(barrierCount, barriers);
	}
}

bool FFFrameInterpolatorDX::LoadTextureFromNGXParameters(
	NGXInstanceParameters *NGXParameters,
	const char *Name,
	FfxResource *OutFfxResource,
	FfxResourceStates State)
{
	ID3D12Resource *resource = nullptr;
	NGXParameters->GetVoidPointer(Name, reinterpret_cast<void **>(&resource));

	if (!resource)
	{
		*OutFfxResource = {};
		return false;
	}

	*OutFfxResource = ffxGetResourceDX12(resource, ffxGetResourceDescriptionDX12(resource), nullptr, State);
	return true;
}
