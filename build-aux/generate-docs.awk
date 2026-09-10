# POSIX awk. Usage: awk -v mode=dzen|markdown|man|examples [-v outdir=DIR] -f FILE SOURCE
# README.dzen uses doubled carets for displayed commands and single carets for
# live commands. dzen adds viewer styling; markdown emits copyable references;
# man emits Markdown with font markers for generate-man.sh and Pandoc;
# examples validates Input/Result pairs and writes Result files under outdir.
#
# Documentation pipeline (ordinary make runs both text and screenshot updates):
#   README.dzen -> markdown -> README.md
#   README.dzen -> man -> build/doc-man.md -> generate-man.sh -> dzen2.1
#     generate-man.sh runs Pandoc, then replaces font markers with roff escapes.
#   README.dzen -> dzen -> dzen2-help's live dzen2 window
#   README.dzen -> examples -> IDENTIFIER.txt files and an ordered manifest
#
# Example blocks require a unique ID, four-space-indented Input/Result and blank
# lines between blocks. Decoding Input must reproduce Result exactly. Markdown
# shows copyable Input plus a PNG link; the viewer renders Result unchanged.
#
# doc-screenshots.sh compares extracted text with docs/screenshots/IDENTIFIER.txt.
# Only new/changed inputs or missing PNGs need capture-doc-screenshots.sh (Xvfb).
# Successful captures replace the PNG and its text cache; prose edits skip capture.
# The generated manifest tracks current IDs for cleanup, installation and removal.
# Keep PNGs, text caches and manifest together in the repository. Out-of-tree
# builds reuse matching distributed assets and write changes in the build tree.
#
# Focused commands: make update-docs (README/man), make doc-screenshots (images),
# make test-docs (literal HEREDOC expectations, validation and screenshot caching).

function fail(message) { print FILENAME ": " message > "/dev/stderr"; exit 1 }
# Markers survive Pandoc even in code blocks; generate-man.sh replaces them
# with roff font escapes afterwards. Always close a style before opening another.
function bold(s) { return mode == "dzen" ? "^fg(lightblue)" s "^fg()" : "_DZENFMTBOLDTOKEN" s "_DZENFMTRESETTOKEN" }
function italic(s) {
    if (s == "") return ""
    return mode == "dzen" ? "^underline()" s "^underline(off)" : "_DZENFMTITALICTOKEN" s "_DZENFMTRESETTOKEN"
}
# s already has one leading caret. Restore its escape only for the live viewer,
# and treat the entire argument (including spaces/separators) as one styled span.
function command(s,    p,name,arg) {
    p=index(s,"("); name=substr(s,1,p); arg=substr(s,p+1,length(s)-p-1)
    if (mode == "dzen") name="^" name
    if (!length(arg)) return bold(name ")")
    return bold(name) italic(arg) bold(")")
}
# CLI references split at the first space; event/action names have no argument.
function reference(s,    p) {
    if (s ~ /^-[[:alnum:]][[:alnum:]-]* /) {
        p=index(s," "); return bold(substr(s,1,p-1)) " " italic(substr(s,p+1))
    }
    return bold(s)
}
# Consume original characters only: generated escapes are never scanned again.
# code suppresses automatic Markdown backticks within literal blocks; inside
# prevents recursive parsing of the backticks belonging to an existing span.
function render(s,code,inside,    r,i,j,k,c,t,run,body,ref) {
    r=""
    for (i=1;i<=length(s);) {
        c=substr(s,i,1)
        if (c=="`" && !inside) {
            # Match the delimiter run so a double-backtick span can contain `.
            run="`"; j=i+1
            while (substr(s,j,1)=="`") { run=run "`"; j++ }
            k=index(substr(s,j),run)
            if (k) {
                body=substr(s,j,k-1)
                t=render(body,1,1)
                # In literal blocks only reference notation loses its backticks.
                # A leading span followed by a description is a reference row.
                ref=(body ~ /^-[[:alnum:]][[:alnum:]-]*( .*)?$/ || body ~ /^dzen2(-help)?$/ ||
                     body ~ /^\^\^[[:alpha:]]+\([^)]*\)$/ ||
                     (substr(s,1,i-1) ~ /^[ \t]*$/ && substr(s,j+k-1+length(run)) ~ /^(  |:)/))
                if (code && !ref) r=r run t run
                else if (mode=="dzen" || mode=="man") {
                    if (body !~ /\^/) t=reference(t)
                    r=r t
                } else if (!code) r=r run t run
                else r=r t
                i=j+k-1+length(run); continue
            }
        }
        if (substr(s,i,2)=="^^") {
            j=i
            while (substr(s,j,1)=="^") j++
            # Only a pair, never the tail of a longer run, starts a reference.
            if (j-i==2 && match(substr(s,i+2),/^[[:alpha:]]+\([^)]*\)/)) {
                t="^" substr(s,i+2,RLENGTH)
                if (mode=="dzen" || mode=="man") r=r command(t)
                else r=r ((!code && !inside) ? "`" t "`" : t)
                i+=length(t)+1; continue
            }
            for (k=i;k<j;k+=2) r=r (mode=="dzen" ? substr(s,k,(j-k>=2 ? 2 : 1)) : "^")
            i=j; continue
        }
        # In particular, a single caret/live command passes through untouched.
        r=r c; i++
    }
    return r
}
# Input is stricter than prose: every caret must be escaped. Decode once and
# compare byte for byte with Result; never execute the example as a shell script.
function decode(s,    r,i) {
    r=""
    for(i=1;i<=length(s);i++) {
        if(substr(s,i,1)=="^") {
            if(substr(s,i+1,1)!="^") fail("every caret in Input must be doubled")
            i++
        }
        r=r substr(s,i,1)
    }
    return r
}
# Screenshot examples may render static content but must not mutate windows.
# Escaped carets in Result are literal text, so skip pairs before checking names.
function controls(s,    i,t) {
    for(i=1;i<=length(s);i++) if(substr(s,i,1)=="^") {
        if(substr(s,i+1,1)=="^") { i++; continue }
        t=substr(s,i)
        if(t ~ ("^\\^(border|padding|normfg|normbg|normfn|tw|cs|collapse|uncollapse|togglecollapse|" \
                "hide|unhide|togglehide|stick|unstick|togglestick|raise|lower|scrollhome|scrollend|exit)\\("))
            fail("window control is not allowed in Result")
    }
}
# Buffer text output until validation completes. The lines array allows heading
# lookahead and consuming a complete Example/Input/Result group as one unit.
function emit(s) { if(mode!="examples") output=output s "\n" }
{ lines[NR]=$0 }
END {
    if(mode!="dzen" && mode!="markdown" && mode!="man" && mode!="examples") fail("invalid mode")
    for(n=1;n<=NR;n++) {
        line=lines[n]
        f=line; sub(/^   ? ?/,"",f)
        # Fenced content takes precedence over Example and heading recognition.
        # A closing fence needs the same character and at least the opening length.
        if(fence!="" || f ~ /^```/ || f ~ /^~~~/) {
            if(fence=="") { match(f,/^(`+|~+)/); fence=substr(f,1,RLENGTH); emit(line) }
            else if(substr(f,1,1)==substr(fence,1,1) && match(f,/^(`+|~+)/) &&
                    RLENGTH>=length(fence) && substr(f,RLENGTH+1) ~ /^[ \t]*$/) { fence=""; emit(line) }
            else emit(render(line,1,0))
            continue
        }
        if(line ~ /^Example:/) {
            # IDs become filenames: restrict their syntax and reject duplicates.
            if(line !~ /^Example: [a-z0-9]+(-[a-z0-9]+)* — .+$/) fail("invalid Example heading")
            id=line; sub(/^Example: /,"",id); sub(/ — .*/,"",id)
            title=line; sub(/^.* — /,"",title)
            if(seen[id]++) fail("duplicate example: " id)
            start=n; count=0
            # Each block has four structural spaces and ends with a blank line
            # (or EOF for Result). Extra indentation belongs to the example data.
            while(lines[++n]=="" && n<=NR) {}
            if(lines[n]!="Input:") fail(id ": expected Input:")
            while(++n<=NR && lines[n] ~ /^    /) {
                inputs[++count]=substr(lines[n],5); decoded[count]=decode(inputs[count])
            }
            if(!count || (n<=NR && lines[n]!="")) fail(id ": empty or unterminated Input")
            while(lines[n]=="" && n<=NR) n++
            if(lines[n]!="Result:") fail(id ": expected Result:")
            resultstart=n; resultcount=0
            while(++n<=NR && lines[n] ~ /^    /) {
                result=substr(lines[n],5); resultcount++
                if(resultcount>count || result!=decoded[resultcount]) fail(id ": Input must decode to Result exactly")
                controls(result)
            }
            if(resultcount!=count || (n<=NR && lines[n]!="")) fail(id ": empty or unterminated Result")
            if(mode=="examples") {
                if(outdir=="") fail("outdir is required")
                path=outdir "/" id ".txt"
                for(j=1;j<=count;j++) print decoded[j] > path
                close(path); manifest=manifest id "\n"
            } else if(mode=="dzen") {
                # Style only Input; Result must remain live, unchanged dzen data.
                for(j=start;j<n;j++)
                    emit(j>start && lines[j] ~ /^    / && j<resultstart ? render(lines[j],1,0) : lines[j])
            } else {
                # Published documents show copyable Input instead of live Result;
                # Markdown also links the corresponding reviewed screenshot.
                emit((mode=="man" ? "## " : "### ") title); emit(""); emit("```text")
                for(j=1;j<=count;j++) emit(mode=="man" ? render(inputs[j],1,0) : decoded[j])
                emit("```"); emit("")
                if(mode=="markdown") { emit("![" title "](docs/screenshots/" id ".png)"); emit("") }
            }
            n--; continue
        }
        if(n<NR && lines[n+1] ~ /^[=-][=-][=-]+$/ && line !~ /^[ \t]/) {
            # Setext headings stay readable in dzen; man uses one fewer # than
            # README Markdown. Indented headings remain literal code-block text.
            if(mode=="dzen") { emit("^fg(#6fbf47)" line "^fg()"); emit("^fg(#6fbf47)" lines[n+1] "^fg()") }
            else emit((mode=="man" ? "" : "#") (substr(lines[n+1],1,1)=="=" ? "# " : "## ") line)
            n++; continue
        }
        emit(render(line,line ~ /^(    |\t)/,0))
    }
    # Preserve source order in the screenshot manifest and one final newline in
    # text documents, regardless of trailing blank lines in the source.
    if(mode=="examples") { printf "%s",manifest > (outdir "/manifest"); close(outdir "/manifest") }
    else { sub(/\n+$/,"",output); print output }
}
