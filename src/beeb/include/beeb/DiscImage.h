#ifndef HEADER_DE589E689A9A443B974DE1B83091A8CD // -*- mode:c++ -*-
#define HEADER_DE589E689A9A443B974DE1B83091A8CD

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <memory>
#include <string>

class FileDialog;
struct LogSet;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//
// There are two types of DiscImage pointer:
//
// 1. Owning pointer: shared_ptr<DiscImage>. There should be at most
// one of these, most likely one of the disc image pointers in a
// BBCMicro object.
//
// 2. Viewing pointer: weak_ptr<const DiscImage> or shared_ptr<const
// DiscImage>. There can be any number of these. (shared_ptr<const
// DiscImage> are of course owning pointers as well, but all you can
// do with that DiscImage is Clone it.)
//
// DiscImage derives from enable_shared_from_this, so there's at least
// only one canonical control block for it.
//
// The hope is that if most public API-type stuff hands out one of the
// viewing pointers, which are hard to convert into an owning pointer,
// this'll at least make it somewhat difficult for me to get it
// wrong...
//
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct DiscImageSummary {
    std::string name;
    std::string load_method;
    std::string description;
    std::string hash;
};

class DiscImage : public std::enable_shared_from_this<DiscImage> {
  public:
    DiscImage();
    virtual ~DiscImage() = 0;

    // returns nullptr if disc_image is null, or if the image can't be
    // cloned.
    static std::shared_ptr<DiscImage> Clone(const std::shared_ptr<DiscImage> &disc_image);
    static std::shared_ptr<DiscImage> Clone(const std::shared_ptr<const DiscImage> &disc_image);

    // default impl returns false.
    virtual bool CanClone() const;

    // default impl returns false.
    virtual bool CanSave() const;

    DiscImageSummary GetSummary() const;

    virtual std::shared_ptr<DiscImage> Clone() const = 0;

    virtual std::string GetHash() const = 0;

    // The name the disc image was loaded with.
    virtual std::string GetName() const = 0;

    // The method used to load the disc image - a string (that ought
    // to be vaguely meaningful to a human). The disc image doesn't do
    // anything with this information except note it.
    virtual std::string GetLoadMethod() const = 0;

    virtual std::string GetDescription() const = 0;

    // Adds file dialog filter suitable for selecting files of
    // whatever type of disc image this image was loaded from. Use
    // this to populate a save dialog when saving a copy.
    virtual void AddFileDialogFilter(FileDialog *fd) const = 0;

    // Save a copy of this disc image to the given file. If
    // successful, returns a clone with the new name and whatever load
    // method indicates a file.
    virtual bool SaveToFile(const std::string &file_name, const LogSet &logs) const = 0;

    //
    virtual bool Read(uint8_t *value, uint8_t side, uint8_t track, uint8_t sector, size_t offset) const = 0;
    virtual bool Write(uint8_t side, uint8_t track, uint8_t sector, size_t offset, uint8_t value) = 0;
    virtual void Flush() = 0;

    virtual bool GetDiscSectorSize(size_t *size, uint8_t side, uint8_t track, uint8_t sector, bool double_density) const = 0;
    virtual bool IsWriteProtected() const = 0;

  protected:
    // Derived class can exposed a derived default if required.
    DiscImage(const DiscImage &) = default;
    DiscImage &operator=(const DiscImage &) = default;

    DiscImage(DiscImage &&) = default;
    DiscImage &operator=(DiscImage &&) = default;

  private:
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
