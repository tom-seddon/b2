#ifndef HEADER_213C9EB267114EF4B71A8A8184599B19 // -*- mode:c++ -*-
#define HEADER_213C9EB267114EF4B71A8A8184599B19

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// Write-through disc image.
//
// Opens disk image file as required, then closes on motor spin down. This
// strikes a decent balance between performance (fopen per byte is pretty
// terrible on Windows...) and making it easy to rewrite a DFS disk image that's
// in use (file can be overwritten when motor is off).
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <beeb/DiscImage.h>
#include "DiscGeometry.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class DirectDiscImage : public DiscImage {
  public:
    static const std::string LOAD_METHOD_DIRECT;

    ~DirectDiscImage();

    static std::shared_ptr<DirectDiscImage> CreateForFile(std::string path, const LogSet &logs);

    DirectDiscImage(const DirectDiscImage &) = delete;
    DirectDiscImage &operator=(const DirectDiscImage &) = delete;

    DirectDiscImage(DirectDiscImage &&) = delete;
    DirectDiscImage &operator=(DirectDiscImage &&) = delete;

    bool CanClone() const override;
    bool CanSave() const override;
    std::shared_ptr<DiscImage> Clone() const override;

    std::string GetHash() const override;
    std::string GetName() const override;
    std::string GetLoadMethod() const override;
    std::string GetDescription() const override;

    std::vector<FileDialogFilter> GetFileDialogFilters() const override;

    bool SaveToFile(const std::string &file_name, const LogSet &logs) const override;

    bool Read(uint8_t *value, uint8_t side, uint8_t track, uint8_t sector, size_t offset) const override;
    bool Write(uint8_t side, uint8_t track, uint8_t sector, size_t offset, uint8_t value) override;
    void Flush() override;

    bool GetDiscSectorSize(size_t *size, uint8_t side, uint8_t track, uint8_t sector, bool double_density) const override;
    bool IsWriteProtected() const override;

  protected:
  private:
    std::string m_path;
    DiscGeometry m_geometry;
    bool m_write_protected;
    mutable FILE *m_fp = nullptr;
    mutable bool m_fp_write = false;

    DirectDiscImage(std::string path, const DiscGeometry &geometry, bool write_protected);
    bool fopenAndSeek(bool write, uint8_t side, uint8_t track, uint8_t sector, size_t offset) const;
    void Close() const;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif