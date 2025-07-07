#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>

void check_error(int error, const char *message) {
    if (error < 0) {
        const git_error *e = git_error_last();
        printf("Error %d: %s\n", error, message);
        if (e) {
            printf("libgit2 error: %s\n", e->message);
        }
        exit(1);
    }
}

void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n", program_name);
    printf("Create, add, and commit a file to a git repository using libgit2\n\n");
    printf("Options:\n");
    printf("  -r, --repo PATH     Path to git repository (default: current directory)\n");
    printf("  -f, --file NAME     Name of file to create (default: example.txt)\n");
    printf("  -m, --message MSG   Commit message (default: auto-generated)\n");
    printf("  -h, --help          Display this help and exit\n");
    printf("\nExamples:\n");
    printf("  %s                           # Use current directory\n", program_name);
    printf("  %s -r /path/to/repo          # Use specific repository\n", program_name);
    printf("  %s -f test.txt -m \"Add test\" # Custom file and message\n", program_name);
}

int main(int argc, char *argv[]) {
    git_repository *repo = NULL;
    git_index *index = NULL;
    git_signature *sig = NULL;
    git_tree *tree = NULL;
    git_oid tree_id, commit_id;
    git_object *parent = NULL;
    const git_commit *parent_commit = NULL;
    FILE *file;
    int error;

    // Default options
    const char *repo_path = ".";
    const char *filename = "example.txt";
    const char *commit_message = NULL;

    // Long options structure
    static struct option long_options[] = {
        {"repo",    required_argument, 0, 'r'},
        {"file",    required_argument, 0, 'f'},
        {"message", required_argument, 0, 'm'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };

    // Parse command line options
    int option_index = 0;
    int c;

    while ((c = getopt_long(argc, argv, "r:f:m:h", long_options, &option_index)) != -1) {
        switch (c) {
            case 'r':
                repo_path = optarg;
                break;
            case 'f':
                filename = optarg;
                break;
            case 'm':
                commit_message = optarg;
                break;
            case 'h':
                print_usage(argv[0]);
                return 0;
            case '?':
                print_usage(argv[0]);
                return 1;
            default:
                abort();
        }
    }

    // Generate default commit message if not provided
    char default_message[256];
    if (!commit_message) {
        snprintf(default_message, sizeof(default_message),
                "Add %s file\n\nThis commit was created using libgit2.", filename);
        commit_message = default_message;
    }

    // Initialize libgit2
    git_libgit2_init();

    printf("Repository path: %s\n", repo_path);
    printf("File to create: %s\n", filename);

    // Open the repository (or create if it doesn't exist)
    error = git_repository_open(&repo, repo_path);
    if (error < 0) {
        printf("Repository not found at '%s'. Creating a new one...\n", repo_path);
        error = git_repository_init(&repo, repo_path, 0);
        check_error(error, "Failed to create repository");
    }

    // Change to repository directory for file operations
    char original_dir[1024];
    if (getcwd(original_dir, sizeof(original_dir)) == NULL) {
        printf("Warning: Could not get current directory\n");
    }

    if (chdir(repo_path) != 0) {
        printf("Failed to change to repository directory: %s\n", repo_path);
        goto cleanup;
    }

    // Create a sample file
    const char *content = "Hello, World!\nThis is a test file created by libgit2.\n";

    file = fopen(filename, "w");
    if (!file) {
        printf("Failed to create file\n");
        goto cleanup;
    }
    fprintf(file, "%s", content);
    fclose(file);
    printf("Created file: %s\n", filename);

    // Get the repository index
    error = git_repository_index(&index, repo);
    check_error(error, "Failed to get repository index");

    // Add the file to the index
    error = git_index_add_bypath(index, filename);
    check_error(error, "Failed to add file to index");

    // Write the index
    error = git_index_write(index);
    check_error(error, "Failed to write index");
    printf("Added file to staging area\n");

    // Create a tree from the index
    error = git_index_write_tree(&tree_id, index);
    check_error(error, "Failed to write tree");

    error = git_tree_lookup(&tree, repo, &tree_id);
    check_error(error, "Failed to lookup tree");

    // Create a signature for the commit
    error = git_signature_now(&sig, "Your Name", "your.email@example.com");
    check_error(error, "Failed to create signature");

    // Try to get the HEAD commit as parent (for subsequent commits)
    error = git_revparse_single(&parent, repo, "HEAD");
    if (error == 0) {
        parent_commit = (const git_commit *)parent;
    }

    // Create the commit
    if (parent_commit) {
        // Subsequent commit with parent
        error = git_commit_create_v(&commit_id, repo, "HEAD", sig, sig,
                                   NULL, commit_message, tree, 1, parent_commit);
    } else {
        // Initial commit (no parent)
        error = git_commit_create_v(&commit_id, repo, "HEAD", sig, sig,
                                   NULL, commit_message, tree, 0);
    }
    check_error(error, "Failed to create commit");

    // Print commit hash
    char commit_hash[GIT_OID_HEXSZ + 1];
    git_oid_tostr(commit_hash, sizeof(commit_hash), &commit_id);
    printf("Created commit: %s\n", commit_hash);
    printf("Commit message: %s\n", commit_message);

cleanup:
    // Restore original directory
    if (strlen(original_dir) > 0) {
        chdir(original_dir);
    }
    // Clean up
    if (parent) git_object_free(parent);
    if (tree) git_tree_free(tree);
    if (sig) git_signature_free(sig);
    if (index) git_index_free(index);
    if (repo) git_repository_free(repo);

    git_libgit2_shutdown();

    return 0;
}
