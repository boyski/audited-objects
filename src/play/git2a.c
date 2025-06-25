#include <git2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void check_error(int error, const char *message);

void
check_error(int error, const char *message) {
    if (error < 0) {
        const git_error *e = git_error_last();
        printf("Error %d: %s\n", error, message);
        if (e) {
            printf("libgit2 error: %s\n", e->message);
        }
        exit(1);
    }
}

int
main(void) {
    git_repository *repo = NULL;
    git_index *index = NULL;
    git_signature *sig = NULL;
    git_tree *tree = NULL;
    git_oid tree_id, commit_id;
    git_object *parent = NULL;
    const git_commit *parent_commit = NULL;
    FILE *file;
    int error;
    
    // Initialize libgit2
    git_libgit2_init();
    
    // Open the repository (assumes current directory is a git repo)
    // To create a new repo instead, use: git_repository_init(&repo, ".", 0);
    error = git_repository_open(&repo, ".");
    if (error < 0) {
        printf("Repository not found. Creating a new one...\n");
        error = git_repository_init(&repo, ".", 0);
        check_error(error, "Failed to create repository");
    }
    
    // Create a sample file
    const char *filename = "example.txt";
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
    const char *commit_message = "Add example.txt file\n\nThis commit was created using libgit2.";
    
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
    // Clean up
    if (parent) git_object_free(parent);
    if (tree) git_tree_free(tree);
    if (sig) git_signature_free(sig);
    if (index) git_index_free(index);
    if (repo) git_repository_free(repo);
    
    git_libgit2_shutdown();
    
    return 0;
}
