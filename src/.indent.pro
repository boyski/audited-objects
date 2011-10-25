// This GNU-indent config file defines the default C coding style
// for this application. If you have access to GNU indent it's best
// to run new code through it to be normalized, especially before
// submitting a patch or other fix.
// NOTE: written for use with indent 2.2.9. There is a known bug
// with 2.2.10 involving --blank-lines-after-declarations.

// This is a variant of K&R style (indent -kr ...).

--blank-lines-after-declarations
--blank-lines-after-procedures
--break-before-boolean-operator
--no-blank-lines-after-commas
--no-comment-delimiters-on-blank-lines
--leave-optional-blank-lines
--braces-on-if-line
--braces-on-struct-decl-line
--comment-indentation 33
--declaration-comment-column 33
--else-endif-column 33
--cuddle-else
--continuation-indentation 4
--case-indentation 4
--no-space-after-casts
--line-comments-indentation 0
--declaration-indentation 1
--dont-format-first-column-comments
--dont-format-comments
--honour-newlines
--indent-level 4
--parameter-indentation 0
--line-length 80
--dont-line-up-parentheses
--continue-at-parentheses
--no-space-after-parentheses
--no-space-after-function-call-names
--procnames-start-lines
--space-after-for
--space-after-if
--space-after-while
--dont-star-comments
--dont-space-special-semicolon

-T size_t
-T ssize_t
-T off_t
-T time_t
-T int32_t
-T uint32_t
-T int64_t
-T uint64_t
-T intptr_t
-T u_short
-T UINT16
-T UINT32
-T FILE
-T CURL
-T CS
-T CCS
-T ca_o
-T ck_o
-T pa_o
-T ps_o
-T pn_o
-T pcode_accumulator_s
-T moment_s
-T op_t
-T prop_t
-T next
-T DWORD
-T HANDLE
-T PIMAGE_DOS_HEADER
-T PIMAGE_FILE_HEADER
