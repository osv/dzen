#!/usr/bin/env python3
"""Project-specific README.dzen reader; never execute example contents."""

import argparse
from pathlib import Path
import re

STYLE_TAGS = {
    "^fg(lightblue)": "DZENFMTBOLDTOKEN",
    "^fg()": "DZENFMTRESETTOKEN",
    "^underline()": "DZENFMTITALICTOKEN",
    "^underline(off)": "DZENFMTRESETTOKEN",
}


def man_text(text):
    # Markers survive Pandoc's code/prose handling; generate-man.sh translates
    # them to roff font escapes afterwards, including inside literal examples.
    active = None

    def convert(match):
        nonlocal active
        token = match[0]
        if token == "^^":
            return "^"
        if token in ("^fg(lightblue)", "^underline()"):
            prefix = STYLE_TAGS["^fg()"] if active else ""
            active = "color" if token.startswith("^fg") else "underline"
            return prefix + STYLE_TAGS[token]
        if ((token.startswith("^fg(") and active == "color") or
                (token == "^underline(off)" and active == "underline")):
            active = None
            return STYLE_TAGS["^fg()"]
        return ""

    result = re.sub(r"\^\^|\^[a-zA-Z]+\([^)]*\)", convert, text)
    return result + (STYLE_TAGS["^fg()"] if active else "")


def input_text(text):
    def decode(match):
        token = match[0]
        if token == "^^":
            return "^"
        if token in STYLE_TAGS:
            return ""
        raise ValueError("every caret in Input must be doubled, except documentation style tags")
    return re.sub(r"\^\^|\^[a-zA-Z]+\([^)]*\)|\^", decode, text)


def plain(text):
    """Remove live commands and unescape literal carets, in one pass."""
    return re.sub(r"\^\^|\^[a-zA-Z]+\([^)]*\)",
                  lambda m: "^" if m[0] == "^^" else "", text)


def markdown_prose(text):
    """Wrap complete command references, preserving existing inline code."""
    command = re.compile(
        r"(?<![\w^])\^[a-zA-Z]+\([^)]*\)|"
        r"\bdzen2(?:-help)?\b(?![\w.-])|"
        r"(?<![\w-])-(?:fg|bg|fn|b|pad|underline|overline|ta|sa|x|y|w|tw|h|"
        r"geometry|expand|dock|title-name|slave-name|l|m|e|p|u|xs|output|lm|v|fn-preload)"
        r"(?![\w-])(?:[ \t]+(?:[A-Z][A-Z0-9]*(?:\[,COLOR\])?|"
        r"WxH\+X\+Y|\[SECONDS\]|l\|c\|r|\[v\|h\]|left\|center\|right)(?![\w]))?"
    )
    # Backtick runs, rather than single backticks, also protect code containing
    # literal backticks. Do not interpret commands inside an existing span.
    pieces = []
    start = 0
    for match in re.finditer(r"(`+)(?!`)(.+?)(?<!`)\1(?!`)", text):
        pieces.append(command.sub(lambda m: "`" + m[0] + "`", text[start:match.start()]))
        pieces.append(match[0])
        start = match.end()
    pieces.append(command.sub(lambda m: "`" + m[0] + "`", text[start:]))
    return "".join(pieces)


def parse(source):
    lines = source.splitlines()
    readme, manual, examples = [], [], {}
    i = 0
    fence = None
    while i < len(lines):
        line = lines[i]
        decoded = plain(line)
        fence_match = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", decoded)
        if fence or fence_match:
            readme.append(decoded)
            manual.append(man_text(line))
            if fence:
                if (fence_match and fence_match[1][0] == fence[0] and
                        len(fence_match[1]) >= len(fence) and not fence_match[2].strip()):
                    fence = None
            else:
                fence = fence_match[1]
            i += 1
            continue
        if line.startswith("Example:"):
            match = re.fullmatch(r"Example: ([a-z0-9]+(?:-[a-z0-9]+)*) — (.+)", line)
            if not match:
                raise ValueError(f"line {i + 1}: invalid Example heading")
            ident, title = match.groups()
            if ident in examples:
                raise ValueError(f"duplicate example: {ident}")
            blocks = {}
            i += 1
            for label in ("Input:", "Result:"):
                while i < len(lines) and not lines[i]:
                    i += 1
                if i == len(lines) or lines[i] != label:
                    raise ValueError(f"{ident}: expected {label}")
                i += 1
                block = []
                while i < len(lines) and lines[i].startswith("    "):
                    block.append(lines[i][4:])
                    i += 1
                if not block or (i < len(lines) and lines[i]):
                    raise ValueError(f"{ident}: empty or unterminated {label} block")
                blocks[label] = block
            code = [input_text(s) for s in blocks["Input:"]]
            result = blocks["Result:"]
            if code != result:
                raise ValueError(f"{ident}: Input must decode to Result exactly")
            if any(re.search(r"(?<!\^)\^(?:border|padding|normfg|normbg|normfn|tw|cs|"
                             r"collapse|uncollapse|togglecollapse|hide|unhide|togglehide|"
                             r"stick|unstick|togglestick|raise|lower|scrollhome|scrollend|exit)\(", s)
                   for s in result):
                raise ValueError(f"{ident}: window control is not allowed in Result")
            examples[ident] = result
            common = [f"### {title}", "", "```text", *code, "```", ""]
            readme.extend(common + [f"![{title}](docs/screenshots/{ident}.png)", ""])
            manual.extend([f"## {title}", "", "```text",
                           *[man_text(s) for s in blocks["Input:"]], "```", ""])
            continue
        # The dzen-readable setext headings also become proper Markdown headings.
        code_line = decoded.startswith(("    ", "\t"))
        heading = (not code_line and i + 1 < len(lines) and
                   re.fullmatch(r"[=-]{3,}", plain(lines[i + 1])))
        if heading:
            level = "##" if plain(lines[i + 1]).startswith("=") else "###"
            rendered = f"{level} {plain(line)}"
            i += 2
        else:
            rendered = decoded if code_line else markdown_prose(decoded)
            i += 1
        readme.append(rendered)
        manual.append(rendered[1:] if heading else man_text(line))
    return "\n".join(readme).rstrip() + "\n", "\n".join(manual).rstrip() + "\n", examples


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--readme", type=Path)
    parser.add_argument("--man", type=Path)
    parser.add_argument("--examples", type=Path)
    args = parser.parse_args()
    try:
        readme, manual, examples = parse(args.source.read_text(encoding="utf-8"))
    except ValueError as exc:
        parser.error(str(exc))
    for path, content in ((args.readme, readme), (args.man, manual)):
        if path:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")
    if args.examples:
        args.examples.mkdir(parents=True, exist_ok=True)
        for ident, lines in examples.items():
            (args.examples / f"{ident}.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")
        (args.examples / "manifest").write_text("\n".join(examples) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
