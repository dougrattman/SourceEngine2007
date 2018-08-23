// Copyright © 1996-2018, Valve Corporation, All rights reserved.

#include "base/include/windows/windows_light.h"

#include "include/ibink_media_player.h"

#include "deps/bink/bink.h"
#undef u8
#undef u16
#undef u32
#undef u64
#undef f32
#undef f64

source::IBinkMediaPlayer::~IBinkMediaPlayer() {}

namespace {
class ScopedBinkBufferLock {
 public:
  ScopedBinkBufferLock(HBINKBUFFER bink_buffer)
      : bink_buffer_{bink_buffer},
        is_locked_{BinkBufferLock(bink_buffer) != 0} {}

  ~ScopedBinkBufferLock() {
    if (IsLocked()) BinkBufferUnlock(bink_buffer_);
  }

  bool IsLocked() const noexcept { return is_locked_; }

 private:
  const HBINKBUFFER bink_buffer_;
  const bool is_locked_;

  ScopedBinkBufferLock(ScopedBinkBufferLock &l) = delete;
  ScopedBinkBufferLock &operator=(ScopedBinkBufferLock &l) = delete;
};

class BinkBuffer {
  friend class BinkMediaPlayer;

 public:
  BinkBuffer(HWND window, u32 width, u32 height, u32 buffer_flags)
      : window_{window},
        bink_buffer_{BinkBufferOpen(window, width, height, buffer_flags)} {}

  ~BinkBuffer() noexcept {
    // Close the Bink buffer.
    if (IsOpened()) BinkBufferClose(bink_buffer_);
  }

  bool IsOpened() const noexcept { return bink_buffer_ != nullptr; }
  const ch *GetLastError() const noexcept { return BinkBufferGetError(); }

  void Blit(BINKRECT *rects, u32 num_rects) const noexcept {
    BinkBufferBlit(bink_buffer_, rects, num_rects);
  }

 private:
  const HWND window_;
  const HBINKBUFFER bink_buffer_;

  bool SetWindowScale(u32 width, u32 height) const noexcept {
    return BinkBufferSetScale(bink_buffer_, width, height) != 0;
  }

  void AdjustWindowPos(int &x, int &y) const noexcept {
    BinkBufferCheckWinPos(bink_buffer_, &x, &y);
  }

  bool SetWindowOffset(int x, int y) const noexcept {
    return BinkBufferSetOffset(bink_buffer_, x, y) != 0;
  }

  BinkBuffer(BinkBuffer &l) = delete;
  BinkBuffer &operator=(BinkBuffer &l) = delete;
};

class BinkMediaPlayer : public source::IBinkMediaPlayer {
 public:
  BinkMediaPlayer(const ch *media_path, u32 bink_flags, HWND window,
                  u32 buffer_flags) noexcept
      : bink_{BinkOpen(media_path, bink_flags)},
        bink_buffer_{window, bink_ ? bink_->Width : 0,
                     bink_ ? bink_->Height : 0, buffer_flags} {}

  virtual ~BinkMediaPlayer() noexcept {
    if (IsOpened()) BinkClose(bink_);
  }

  const ch *GetLastError() const noexcept {
    const ch *error{BinkGetError()};

    return error ? error : bink_buffer_.GetLastError();
  }

  bool IsOpened() const noexcept {
    return bink_ != nullptr && bink_buffer_.IsOpened();
  }

  bool HasFrames() const noexcept override {
    return bink_->FrameNum != bink_->Frames;
  }

  bool CanPresent() const noexcept override {
    return BinkWait(bink_) == 0 && HasFrames();
  }

  void Present(bool do_present_old_frame) const noexcept override {
    if (!do_present_old_frame) {
      // Decompress the Bink frame.
      if (DecodeFrame()) {
        // Copy frame to buffer.
        Frame2Buffer();

        // Tell the BinkBuffer to blit the pixels onto the screen (if the
        // BinkBuffer is using an off-screen blitting style).
        Blit2Buffer();

        // Are we at the end of the movie?  Nope, advance to the next frame.
        if (HasFrames()) NextFrame();
      }

      return;
    }

    Blit2Buffer();
  }

  bool Play() const noexcept override { return BinkPause(bink_, 0) == 0; }

  bool Pause() const noexcept override { return BinkPause(bink_, 1) != 0; }

  bool AutoscaleTo(u32 width, u32 height) const noexcept override {
    auto[new_width, new_height] = GetScaledRect(width, height);

    const u32 x_pos = (width - new_width) / 2;
    const u32 y_pos = (height - new_height) / 2;

    return bink_buffer_.SetWindowScale(new_width, new_height) &&
           bink_buffer_.SetWindowOffset(x_pos, y_pos);
  }

  void AdjustWindowPos(int &x, int &y) const noexcept override {
    bink_buffer_.AdjustWindowPos(x, y);
  }

  bool SetWindowOffset(int x, int y) const noexcept override {
    return bink_buffer_.SetWindowOffset(x, y);
  }

 private:
  const HBINK bink_;
  const BinkBuffer bink_buffer_;

  bool DecodeFrame() const noexcept { return BinkDoFrame(bink_) == 0; }

  // Note that if the video is falling behind the audio, then this function may
  // return without copying any pixels.  If the copy is skipped, then this
  // function will return a false.  If copy was successful, then a true
  // will be returned.
  bool Frame2Buffer() const noexcept {
    HBINKBUFFER buffer{bink_buffer_.bink_buffer_};
    // Lock the BinkBuffer so that we can copy the decompressed frame into it.
    ScopedBinkBufferLock lock{buffer};

    // Copy the decompressed frame into the BinkBuffer (this might be
    // on-screen).
    return lock.IsLocked() &&
           BinkCopyToBuffer(bink_, buffer->Buffer, buffer->BufferPitch,
                            buffer->Height, 0, 0, buffer->SurfaceType) == 0;
  }

  void NextFrame() const noexcept { BinkNextFrame(bink_); }

  void Blit2Buffer() const noexcept {
    bink_buffer_.Blit(
        bink_->FrameRects,
        BinkGetRects(bink_, bink_buffer_.bink_buffer_->SurfaceType));
  }

  std::tuple<u32, u32> GetScaledRect(u32 width, u32 height) const noexcept {
    // Integral scaling is much faster, so always scale the video as such

    // Find if we need to scale up or down
    if (bink_->Width < width && bink_->Height < height) {
      // Scaling up by powers of two
      u32 nScale = 0;
      while ((bink_->Width << (nScale + 1)) <= width &&
             (bink_->Height << (nScale + 1)) <= height)
        nScale++;

      return {bink_->Width << nScale, bink_->Height << nScale};
    }

    if (bink_->Width > width && bink_->Height > height) {
      // Scaling down by powers of two
      u32 nScale = 1;
      while ((bink_->Width >> nScale) > width &&
             (bink_->Height >> nScale) > height)
        nScale++;

      return {bink_->Width >> nScale, bink_->Height >> nScale};
    }

    return {bink_->Width, bink_->Height};
  }

  BinkMediaPlayer(BinkMediaPlayer &p) = delete;
  BinkMediaPlayer &operator=(BinkMediaPlayer &p) = delete;
};

class BinkMediaFactory : public CBaseAppSystem<source::IBinkMediaFactory> {
 public:
  void *QueryInterface(const char *name) override {
    if (!strncmp(name, source::kValveAviBinkMediaFactoryInterfaceVersion,
                 std::size(source::kValveAviBinkMediaFactoryInterfaceVersion)))
      return implicit_cast<IBinkMediaFactory *>(this);

    return nullptr;
  }

  std::tuple<std::unique_ptr<source::IBinkMediaPlayer>, const ch *> Open(
      const ch *media_path, u32 bink_flags, HWND window,
      u32 buffer_flags) override {
    // Supplying a nullptr context will cause Bink to allocate its own.
    BinkSoundUseDirectSound(nullptr);

    auto player = std::make_unique<BinkMediaPlayer>(media_path, bink_flags,
                                                    window, buffer_flags);
    if (player->IsOpened()) return {std::move(player), nullptr};

    return {nullptr, player->GetLastError()};
  }
};

static BinkMediaFactory bink_media_factory;
}  // namespace

using namespace source;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR(BinkMediaFactory, IBinkMediaFactory,
                                  kValveAviBinkMediaFactoryInterfaceVersion,
                                  bink_media_factory);