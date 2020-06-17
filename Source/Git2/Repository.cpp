#include "Repository.h"
#include <sstream>

namespace {
    struct Init {
        Init() {
            git_libgit2_init();
        }

        ~Init() {
            git_libgit2_shutdown();
        }
    } _UU_;
}

namespace Git2 {
    void Exception::Raise(int code) {
        const auto construct = [](int code) noexcept {
            auto error = git_error_last();
            Exception exception{error->message};
            exception.mCode = code;
            exception.mKlass = error->klass;
            return exception;
        };
        throw construct(code);
    }

    static void Guard(int code) { if (code != 0) Exception::Raise(code); }

    Repository Repository::Open(const std::filesystem::path &path) {
        const auto abs = std::filesystem::absolute(path);
        Repository result{};
        Guard(git_repository_open(&result.mHandle, abs.generic_string().c_str()));
        return result;
    }

    Repository Repository::Create(const std::filesystem::path &path, const bool isBare) {
        const auto abs = std::filesystem::absolute(path);
        Repository result{};
        Guard(git_repository_init(&result.mHandle, abs.generic_string().c_str(), isBare));
        return result;
    }

    Repository Repository::Clone(const std::filesystem::path &path, const std::string &uri) {
        const auto abs = std::filesystem::absolute(path);
        Repository result{};
        Guard(git_clone(&result.mHandle, uri.c_str(), abs.generic_string().c_str(), nullptr));
        return result;
    }

    void Repository::Fetch(const std::string &origin) {
        git_remote *remote;
        Guard(git_remote_lookup(&remote, mHandle, origin.c_str()));
        git_fetch_options options = GIT_FETCH_OPTIONS_INIT;
        Guard(git_remote_fetch(remote, nullptr, &options, nullptr));
    }

    static int SearchFetchHead(const char *name, const char *url,
                               const git_oid *oid, unsigned int is_merge, void *payload) {
        if (is_merge) {
            memcpy(payload, oid, sizeof(git_oid));
        }
        return 0;
    }

    template<class Fn>
    struct Finally {
        explicit Finally(Fn fn) : fn(std::move(fn)) {}

        ~Finally() { fn(); }

    private:
        Fn fn;
    };

    static void FastForward(git_repository *repo, const git_oid *target_oid, int is_unborn) {
        git_checkout_options ff_checkout_options = GIT_CHECKOUT_OPTIONS_INIT;
        git_reference *target_ref = nullptr;
        git_reference *new_target_ref = nullptr;
        git_object *target = nullptr;
        auto final1 = Finally([&] {
            git_reference_free(target_ref);
            git_reference_free(new_target_ref);
            git_object_free(target);
        });

        if (is_unborn) {
            git_reference *head_ref = nullptr;
            auto final2 = Finally([&] { git_reference_free(head_ref); });
            /* HEAD reference is unborn, lookup manually so we don't try to resolve it */
            Guard(git_reference_lookup(&head_ref, repo, "HEAD"));
            /* Grab the reference HEAD should be pointing to */
            const auto symbolic_ref = git_reference_symbolic_target(head_ref);
            /* Create our master reference on the target OID */
            Guard(git_reference_create(&target_ref, repo, symbolic_ref, target_oid, 0, nullptr));
        } else {
            /* HEAD exists, just lookup and resolve */
            Guard(git_repository_head(&target_ref, repo));
        }
        /* Lookup the target object */
        Guard(git_object_lookup(&target, repo, target_oid, GIT_OBJECT_COMMIT));
        /* Checkout the result so the workdir is in the expected state */
        ff_checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;
        Guard(git_checkout_tree(repo, target, &ff_checkout_options));
        /* Move the target reference to the target OID */
        Guard(git_reference_set_target(&new_target_ref, target_ref, target_oid, nullptr));
    }

    struct MergeOptions {
        const char **heads;
        git_annotated_commit **annotated;
        size_t annotated_count;
        const UserSignature *sign;
    };

    class CreateMergeCommit {
    public:
        CreateMergeCommit(git_repository *repo, git_index *index, const MergeOptions &opts) {
            parents = static_cast<git_commit **>(calloc(opts.annotated_count + 1, sizeof(git_commit *)));

            /* Grab our needed references */
            Guard(git_repository_head(&head_ref, repo));
            ResolveRefish(&merge_commit, repo, opts.heads[0]);

            /* Setup our parent commits */
            Guard(git_reference_peel((git_object **) &parents[0], head_ref, GIT_OBJECT_COMMIT));
            for (int i = 0; i < opts.annotated_count; i++) {
                git_commit_lookup(&parents[i + 1], repo, git_annotated_commit_id(opts.annotated[i]));
            }

            Guard(git_reference_dwim(&merge_ref, repo, opts.heads[0]));
            const auto message = PrepareCommitMessage();
            const auto tree = PrepareCommitTree(repo, index);
            git_signature *sign;
            Guard(git_signature_now(&sign, opts.sign->Name.c_str(), opts.sign->Email.c_str()));

            /* Commit time ! */
            Guard(git_commit_create(&commit_oid,
                                    repo, git_reference_name(head_ref),
                                    sign, sign, nullptr, message.c_str(), tree,
                                    opts.annotated_count + 1, (const git_commit **) parents));
        }


        ~CreateMergeCommit() { free(parents); }
    private:
        git_oid commit_oid{};
        git_reference *merge_ref = nullptr;
        git_annotated_commit *merge_commit{};
        git_reference *head_ref{};
        git_commit ** parents;

        [[nodiscard]] std::string PrepareCommitMessage() const {
            const char *msg_target;
            if (merge_ref != nullptr)
                Guard(git_branch_name(&msg_target, merge_ref));
            else
                msg_target = git_oid_tostr_s(git_annotated_commit_id(merge_commit));
            std::ostringstream ss{};
            ss << "Merge " << (merge_ref ? "branch" : "commit") << " '" << msg_target << "'";
            return ss.str();
        }

        static void ResolveRefish(git_annotated_commit **commit, git_repository *repo, const char *refish) {
            try {
                git_reference* ref;
                Guard(git_reference_dwim(&ref, repo, refish));
                const auto fin = Finally([=]() noexcept { git_reference_free(ref); });
                Guard(git_annotated_commit_from_ref(commit, repo, ref));
            }
            catch (...) {
                git_object* obj;
                Guard(git_revparse_single(&obj, repo, refish));
                const auto fin = Finally([=]() noexcept {git_object_free(obj);});
                Guard(git_annotated_commit_lookup(commit, repo, git_object_id(obj)));
            }
        }

        static git_tree* PrepareCommitTree(git_repository *repo, git_index *index) {
            git_oid tree_oid;
            git_tree *tree {nullptr};
            Guard(git_index_write_tree(&tree_oid, index));
            Guard(git_tree_lookup(&tree, repo, &tree_oid));
            return tree;
        }
    };

    template<int F>
    [[nodiscard]] constexpr bool Flag(const int v) noexcept { return v & F; } // NOLINT

    void Repository::PullAuto(const UserSignature &sign, const std::string &origin) {
        const auto fin1 = Finally([this]() { git_repository_state_cleanup(mHandle); });
        Fetch(origin);
        git_oid branchOidToMerge;
        Guard(git_repository_fetchhead_foreach(mHandle, SearchFetchHead, &branchOidToMerge));
        git_annotated_commit *heads[1];
        Guard(git_annotated_commit_lookup(&heads[0], mHandle, &branchOidToMerge));
        const auto fin2 = Finally([&]() { git_annotated_commit_free(heads[0]); });
        git_merge_analysis_t analysis;
        git_merge_preference_t preference;
        git_merge_analysis(&analysis, &preference, mHandle, (const git_annotated_commit **) heads, 1);
        if (Flag<GIT_MERGE_ANALYSIS_UP_TO_DATE>(analysis)) return;
        if (Flag<GIT_MERGE_ANALYSIS_UNBORN>(analysis) ||
            (Flag<GIT_MERGE_ANALYSIS_FASTFORWARD>(analysis) &&
             !Flag<GIT_MERGE_PREFERENCE_NO_FASTFORWARD>(preference))) {
            const auto target_oid = git_annotated_commit_id(heads[0]);
            return FastForward(mHandle, target_oid, Flag<GIT_MERGE_ANALYSIS_UNBORN>(analysis));
        }
        if (Flag<GIT_MERGE_ANALYSIS_NORMAL>(analysis)) {
            git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
            git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
            merge_opts.flags = 0;
            merge_opts.file_flags = GIT_MERGE_FILE_STYLE_DIFF3;
            checkout_opts.checkout_strategy = GIT_CHECKOUT_FORCE | GIT_CHECKOUT_ALLOW_CONFLICTS;
            if (Flag<GIT_MERGE_PREFERENCE_FASTFORWARD_ONLY>(preference)) {
                throw std::runtime_error("Fast-forward is preferred, but only a merge is possible");
            }
            Guard(git_merge(mHandle, (const git_annotated_commit **) heads, 1, &merge_opts, &checkout_opts));
            git_index *index;
            Guard(git_repository_index(&index, mHandle));
            if (git_index_has_conflicts(index)) {
                throw std::runtime_error("Conflict with upstream. Please resolve externally.");
            }
            if (sign.Name.empty() || sign.Email.empty()) return; // No auto-commit, drop merge
            CreateMergeCommit(mHandle, index, {nullptr, heads, 1, &sign});
        }
    }
}
