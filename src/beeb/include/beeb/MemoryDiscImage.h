#ifndef HEADER_F219CA200C9D4750A635E975749B8322
#define HEADER_F219CA200C9D4750A635E975749B8322

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <beeb/DiscImage.h>
#include <vector>

struct LogSet;
struct DiscGeometry;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

class MemoryDiscImage : public DiscImage {
  public:
    static const std::string LOAD_METHOD_FILE;

    static const uint8_t FILL_BYTE;

    static std::shared_ptr<MemoryDiscImage> LoadFromBuffer(std::string path, std::string load_method, const void *data, size_t data_size, const DiscGeometry &geometry, const LogSet &logs);

    // If the load succeeds, the method will be LOAD_METHOD_FILE or
    // LOAD_METHOD_ZIP.
    //static std::shared_ptr<MemoryDiscImage> LoadFromFile(std::string path, const LogSet &logs);

    ~MemoryDiscImage();

    MemoryDiscImage(const MemoryDiscImage &) = delete;
    MemoryDiscImage &operator=(const MemoryDiscImage &) = delete;

    MemoryDiscImage(MemoryDiscImage &&) = delete;
    MemoryDiscImage &operator=(MemoryDiscImage &&) = delete;

    bool CanClone() const override;
    bool CanSave() const override;

    std::shared_ptr<DiscImage> Clone() const override;

    std::string GetHash() const override;

    std::string GetName() const override;
    std::string GetLoadMethod() const override;
    std::string GetDescription() const override;
    std::vector<FileDialogFilter> GetFileDialogFilters() const override;
    bool SaveToFile(const std::string &file_name, const LogSet &logs) const override;
    //void SetNameAndLoadMethod(std::string name,std::string load_method);

    bool Read(uint8_t *value, uint8_t side, uint8_t track, uint8_t sector, size_t offset) const override;
    bool Write(uint8_t side, uint8_t track, uint8_t sector, size_t offset, uint8_t value) override;
    void Flush() override;

    bool GetDiscSectorSize(size_t *size, uint8_t side, uint8_t track, uint8_t sector, bool double_density) const override;
    bool IsWriteProtected() const override;

  protected:
  private:
    struct Data;

    Data *m_data;
    std::string m_name;
    std::string m_load_method;

    MemoryDiscImage();
    MemoryDiscImage(std::string path, std::string load_method, const void *data, size_t data_size, const DiscGeometry &geometry);

    // doesn't add a new ref - caller must arrange this.
    MemoryDiscImage(Data *data, std::string name, std::string load_method);

    void MakeDataUnique();
    void ReleaseData(Data **data_ptr);
    void SetName(std::string name);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
