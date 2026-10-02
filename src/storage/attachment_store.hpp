#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace evidence_trace::storage {

struct StagedFile {
    std::filesystem::path temporary_path;
    std::int64_t byte_size{0};
    std::string sha256;
};

// A removal is first moved inside the data directory. The database transaction
// can then be rolled back and the file restored if any SQL operation fails.
struct StagedRemoval {
    std::filesystem::path original_relative;
    std::filesystem::path temporary_path;
    std::filesystem::path trash_directory;
};

class AttachmentStore {
public:
    explicit AttachmentStore(std::filesystem::path root,
                             std::int64_t max_file_size = 100 * 1024 * 1024);

    StagedFile stage_file(const std::filesystem::path& original) const;
    StagedFile stage_bytes(const std::vector<unsigned char>& bytes) const;
    void move_to_final(StagedFile& staged, const std::filesystem::path& relative_path) const;
    StagedRemoval stage_removal(const std::filesystem::path& relative_path) const;
    void restore_removal(StagedRemoval& staged) const;
    void discard_removal(StagedRemoval& staged) const;
    void remove(const std::filesystem::path& relative_path) const;
    std::vector<unsigned char> read(const std::filesystem::path& relative_path) const;
    bool exists(const std::filesystem::path& relative_path) const;
    std::string hash(const std::filesystem::path& relative_path) const;
    std::int64_t size(const std::filesystem::path& relative_path) const;
    std::filesystem::path absolute_path(const std::filesystem::path& relative_path) const;
    void cleanup_temporary_files() const;
    // Recover removals left in quarantine if the process stopped between the
    // filesystem move and the database transaction's final outcome. The
    // callback returns true when the database still owns the attachment.
    void recover_staged_removals(const std::function<bool(const std::filesystem::path&)>& is_still_owned) const;

    const std::filesystem::path& root() const { return root_; }
    std::int64_t max_file_size() const { return max_file_size_; }

private:
    std::filesystem::path root_;
    std::int64_t max_file_size_;
};

std::string sha256_bytes(const std::vector<unsigned char>& bytes);

} // namespace evidence_trace::storage
