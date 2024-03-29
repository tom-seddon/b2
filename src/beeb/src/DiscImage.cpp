#include <shared/system.h>
#include <beeb/DiscImage.h>

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static std::shared_ptr<DiscImage> DoClone(const DiscImage *disc_image) {
    if (!disc_image) {
        return nullptr;
    } else if (!disc_image->CanClone()) {
        return nullptr;
    } else {
        return disc_image->Clone();
    }
}

std::shared_ptr<DiscImage> DiscImage::Clone(const std::shared_ptr<DiscImage> &disc_image) {
    return DoClone(disc_image.get());
}

std::shared_ptr<DiscImage> DiscImage::Clone(const std::shared_ptr<const DiscImage> &disc_image) {
    return DoClone(disc_image.get());
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscImage::DiscImage() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscImage::~DiscImage() {
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DiscImage::CanClone() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

bool DiscImage::CanSave() const {
    return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

DiscImageSummary DiscImage::GetSummary() const {
    DiscImageSummary s;

    s.name = this->GetName();
    s.load_method = this->GetLoadMethod();
    s.description = this->GetDescription();
    s.hash = this->GetHash();

    return s;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
