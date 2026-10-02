#include "storage/attachment_store.hpp"

#include "domain/types.hpp"

#include <openssl/evp.h>

#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>

namespace evidence_trace::storage {

namespace {

class Sha256Context {
public:
    Sha256Context() : context_(EVP_MD_CTX_new()) {
        if (context_ == nullptr || EVP_DigestInit_ex(context_, EVP_sha256(), nullptr) != 1) {
            if (context_ != nullptr) EVP_MD_CTX_free(context_);
            throw std::runtime_error("Unable to initialize SHA-256");
        }
    }
    Sha256Context(const Sha256Context&) = delete;
    Sha256Context& operator=(const Sha256Context&) = delete;
    ~Sha256Context() { EVP_MD_CTX_free(context_); }

    void update(const unsigned char* bytes, std::size_t size) {
        if (EVP_DigestUpdate(context_, bytes, size) != 1) throw std::runtime_error("Unable to hash bytes");
    }

    std::string finish() {
        unsigned char digest[EVP_MAX_MD_SIZE]{};
        unsigned int digest_size = 0;
        if (EVP_DigestFinal_ex(context_, digest, &digest_size) != 1) {
            throw std::runtime_error("Unable to finalize SHA-256");
        }
        std::ostringstream output;
        output << std::hex << std::setfill('0');
        for (unsigned int i = 0; i < digest_size; ++i) output << std::setw(2) << static_cast<unsigned int>(digest[i]);
        return output.str();
    }

private:
    EVP_MD_CTX* context_;
};

void ensure_safe_relative(const std::filesystem::path& relative_path) {
    if (relative_path.empty() || relative_path.is_absolute()) {
        throw std::invalid_argument("Attachment path must be a non-empty relative path");
    }
    for (const auto& component : relative_path) {
        if (component == ".." || component == ".") {
            throw std::invalid_argument("Attachment path contains an unsafe component");
        }
    }
}

StagedFile make_temp(const std::filesystem::path& root) {
    std::filesystem::create_directories(root);
    StagedFile staged;
    staged.temporary_path = root / (".tmp-" + domain::new_id());
    return staged;
}

} // namespace

std::string sha256_bytes(const std::vector<unsigned char>& bytes) {
    Sha256Context context;
    if (!bytes.empty()) context.update(bytes.data(), bytes.size());
    return context.finish();
}

AttachmentStore::AttachmentStore(std::filesystem::path root, std::int64_t max_file_size)
    : root_(std::move(root)), max_file_size_(max_file_size) {
    if (max_file_size_ <= 0) throw std::invalid_argument("Attachment size limit must be positive");
    std::filesystem::create_directories(root_);
    std::error_code error;
    std::filesystem::permissions(root_, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, error);
}

StagedFile AttachmentStore::stage_file(const std::filesystem::path& original) const {
    if (!std::filesystem::is_regular_file(original)) {
        throw std::invalid_argument("Evidence file is not a regular file: " + original.string());
    }
    const auto original_size = std::filesystem::file_size(original);
    if (original_size > static_cast<std::uintmax_t>(max_file_size_)) {
        throw std::invalid_argument("Evidence file exceeds the configured size limit");
    }

    auto staged = make_temp(root_);
    try {
        std::ifstream input(original, std::ios::binary);
        std::ofstream output(staged.temporary_path, std::ios::binary | std::ios::trunc);
        if (!input || !output) throw std::runtime_error("Unable to open evidence file for copying");
        Sha256Context hash_context;
        std::vector<unsigned char> buffer(64 * 1024);
        while (input) {
            input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            const auto count = input.gcount();
            if (count <= 0) continue;
            output.write(reinterpret_cast<const char*>(buffer.data()), count);
            if (!output) throw std::runtime_error("Unable to write staged evidence file");
            hash_context.update(buffer.data(), static_cast<std::size_t>(count));
            staged.byte_size += count;
            if (staged.byte_size > max_file_size_) throw std::invalid_argument("Evidence file exceeds the configured size limit");
        }
        output.close();
        staged.sha256 = hash_context.finish();
        return staged;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(staged.temporary_path, ignored);
        throw;
    }
}

StagedFile AttachmentStore::stage_bytes(const std::vector<unsigned char>& bytes) const {
    if (bytes.size() > static_cast<std::size_t>(max_file_size_)) {
        throw std::invalid_argument("Attachment exceeds the configured size limit");
    }
    auto staged = make_temp(root_);
    try {
        std::ofstream output(staged.temporary_path, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Unable to create staged attachment");
        if (!bytes.empty()) output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!output) throw std::runtime_error("Unable to write staged attachment");
        staged.byte_size = static_cast<std::int64_t>(bytes.size());
        staged.sha256 = sha256_bytes(bytes);
        return staged;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(staged.temporary_path, ignored);
        throw;
    }
}

void AttachmentStore::move_to_final(StagedFile& staged, const std::filesystem::path& relative_path) const {
    ensure_safe_relative(relative_path);
    const auto final_path = absolute_path(relative_path);
    std::filesystem::create_directories(final_path.parent_path());
    if (std::filesystem::exists(final_path)) throw std::runtime_error("Attachment destination already exists");
    std::filesystem::rename(staged.temporary_path, final_path);
    staged.temporary_path.clear();
}

StagedRemoval AttachmentStore::stage_removal(const std::filesystem::path& relative_path) const {
    ensure_safe_relative(relative_path);
    const auto source = absolute_path(relative_path);
    std::error_code status_error;
    const auto status = std::filesystem::symlink_status(source, status_error);
    if (status_error || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status)) {
        throw std::runtime_error("Attachment is missing or is not a regular file: " + relative_path.string());
    }

    StagedRemoval staged;
    staged.original_relative = relative_path;
    staged.trash_directory = root_ / (".delete-" + domain::new_id());
    staged.temporary_path = staged.trash_directory / relative_path;
    try {
        std::filesystem::create_directories(staged.temporary_path.parent_path());
        std::filesystem::rename(source, staged.temporary_path);
        return staged;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(staged.trash_directory, ignored);
        throw;
    }
}

void AttachmentStore::restore_removal(StagedRemoval& staged) const {
    if (staged.temporary_path.empty()) return;
    const auto destination = absolute_path(staged.original_relative);
    if (std::filesystem::exists(destination)) throw std::runtime_error("Cannot restore attachment because its original path is occupied: " + staged.original_relative.string());
    try {
        std::filesystem::create_directories(destination.parent_path());
        std::filesystem::rename(staged.temporary_path, destination);
        std::error_code ignored;
        std::filesystem::remove_all(staged.trash_directory, ignored);
        staged.temporary_path.clear();
        staged.trash_directory.clear();
    } catch (...) {
        throw std::runtime_error("Unable to restore staged attachment: " + staged.original_relative.string());
    }
}

void AttachmentStore::discard_removal(StagedRemoval& staged) const {
    if (staged.trash_directory.empty()) return;
    std::error_code error;
    std::filesystem::remove_all(staged.trash_directory, error);
    if (error) throw std::runtime_error("Unable to permanently remove staged attachment: " + staged.original_relative.string() + ": " + error.message());
    staged.temporary_path.clear();
    staged.trash_directory.clear();
}

void AttachmentStore::remove(const std::filesystem::path& relative_path) const {
    auto staged = stage_removal(relative_path);
    discard_removal(staged);
}

std::vector<unsigned char> AttachmentStore::read(const std::filesystem::path& relative_path) const {
    const auto path = absolute_path(relative_path);
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Attachment file is missing");
    const auto file_size = std::filesystem::file_size(path);
    if (file_size > static_cast<std::uintmax_t>(max_file_size_)) throw std::runtime_error("Attachment is too large");
    std::vector<unsigned char> bytes(static_cast<std::size_t>(file_size));
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to read attachment");
    if (!bytes.empty()) input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input && !input.eof()) throw std::runtime_error("Unable to read attachment");
    return bytes;
}

bool AttachmentStore::exists(const std::filesystem::path& relative_path) const {
    return std::filesystem::is_regular_file(absolute_path(relative_path));
}

std::string AttachmentStore::hash(const std::filesystem::path& relative_path) const {
    const auto path = absolute_path(relative_path);
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Attachment file is missing");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Unable to open attachment");
    Sha256Context context;
    std::vector<unsigned char> buffer(64 * 1024);
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0) context.update(buffer.data(), static_cast<std::size_t>(count));
    }
    return context.finish();
}

std::int64_t AttachmentStore::size(const std::filesystem::path& relative_path) const {
    const auto path = absolute_path(relative_path);
    if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Attachment file is missing");
    return static_cast<std::int64_t>(std::filesystem::file_size(path));
}

std::filesystem::path AttachmentStore::absolute_path(const std::filesystem::path& relative_path) const {
    ensure_safe_relative(relative_path);
    const auto root_absolute = std::filesystem::weakly_canonical(root_);
    const auto candidate = std::filesystem::weakly_canonical(root_ / relative_path);
    const auto root_string = root_absolute.generic_string();
    const auto candidate_string = candidate.generic_string();
    if (candidate_string != root_string && candidate_string.rfind(root_string + '/', 0) != 0) {
        throw std::invalid_argument("Attachment path resolves outside the attachment directory");
    }
    return candidate;
}

void AttachmentStore::cleanup_temporary_files() const {
    if (!std::filesystem::exists(root_)) return;
    for (const auto& entry : std::filesystem::directory_iterator(root_)) {
        if (entry.path().filename().string().rfind(".tmp-", 0) == 0) {
            std::error_code ignored;
            std::filesystem::remove_all(entry.path(), ignored);
        }
    }
}

void AttachmentStore::recover_staged_removals(const std::function<bool(const std::filesystem::path&)>& is_still_owned) const {
    if (!std::filesystem::exists(root_)) return;
    for (const auto& entry : std::filesystem::directory_iterator(root_)) {
        const auto entry_status = std::filesystem::symlink_status(entry.path());
        if (!std::filesystem::is_directory(entry_status) || entry.path().filename().string().rfind(".delete-", 0) != 0) continue;

        std::optional<std::filesystem::path> temporary_path;
        for (const auto& nested : std::filesystem::recursive_directory_iterator(entry.path())) {
            const auto status = std::filesystem::symlink_status(nested.path());
            if (std::filesystem::is_directory(status)) continue;
            if (!std::filesystem::is_regular_file(status) || temporary_path.has_value()) {
                throw std::runtime_error("Unable to recover staged attachment quarantine: " + entry.path().string());
            }
            temporary_path = nested.path();
        }
        if (!temporary_path.has_value()) {
            std::error_code ignored;
            std::filesystem::remove_all(entry.path(), ignored);
            continue;
        }

        StagedRemoval staged;
        staged.trash_directory = entry.path();
        staged.temporary_path = *temporary_path;
        staged.original_relative = std::filesystem::relative(*temporary_path, entry.path());
        ensure_safe_relative(staged.original_relative);
        if (is_still_owned(staged.original_relative)) restore_removal(staged);
        else discard_removal(staged);
    }
}

} // namespace evidence_trace::storage
