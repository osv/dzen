#!/usr/bin/env python3
"""Check escaping, branch selection and invalid documentation examples."""
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("generate_docs", ROOT / "build-aux/generate-docs.py")
docs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(docs)

EXAMPLE = """Example: colors — Colors

Input:
    ^^fg(red)Red^^fg() ^^^^

Result:
    ^fg(red)Red^fg() ^^

Following paragraph.
"""


class DocumentationTests(unittest.TestCase):
    def test_escaping(self):
        self.assertEqual(docs.plain("^fg(red)^^fg(blue)^^^^ ^^^^fg(red)"),
                         "^fg(blue)^^ ^^fg(red)")

    def test_outputs(self):
        readme, man, examples = docs.parse(EXAMPLE)
        self.assertIn("```text\n^fg(red)Red^fg() ^^\n```", readme)
        self.assertIn("```text\n^fg(red)Red^fg() ^^\n```", man)
        self.assertIn("docs/screenshots/colors.png", readme)
        self.assertNotIn("![", man)
        self.assertNotIn("Result:", man)
        self.assertIn("Following paragraph.", man)
        self.assertEqual(examples["colors"], ["^fg(red)Red^fg() ^^"])

    def test_invalid_blocks(self):
        for source in (EXAMPLE + EXAMPLE,
                       EXAMPLE.replace("Result:", "Missing:"),
                       EXAMPLE.replace("    ^fg(red)Red^fg() ^^", ""),
                       EXAMPLE.replace("\n\nFollowing", "\nFollowing"),
                       EXAMPLE.replace("    ^fg(red)Red^fg() ^^", "    wrong"),
                       EXAMPLE.replace("colors —", "../colors —")):
            with self.subTest(source=source), self.assertRaises(ValueError):
                docs.parse(source)

    def test_reject_control(self):
        source = EXAMPLE.replace("^^fg(red)Red^^fg() ^^^^", "^^hide()")
        source = source.replace("^fg(red)Red^fg() ^^", "^hide()")
        with self.assertRaisesRegex(ValueError, "window control"):
            docs.parse(source)

    def test_reject_live_input(self):
        with self.assertRaisesRegex(ValueError, "caret"):
            docs.parse(EXAMPLE.replace("^^fg(red)", "^fg(red)"))

    def test_heading_levels(self):
        readme, man, _ = docs.parse("^fg(green)NAME\n^fg(green)====\n\ndzen2\n")
        self.assertTrue(readme.startswith("## NAME\n"))
        self.assertTrue(man.startswith("# NAME\n"))

    def test_reference_styles(self):
        source = "    ^fg(lightblue)-fg^fg() ^underline()COLOR^underline(off)\n"
        readme, man, _ = docs.parse(source)
        self.assertEqual(readme, "    -fg COLOR\n")
        self.assertEqual(man, "    DZENFMTBOLDTOKEN-fgDZENFMTRESETTOKEN "
                         "DZENFMTITALICTOKENCOLORDZENFMTRESETTOKEN\n")
        self.assertEqual(docs.man_text("^fg()normal"), "normal")
        self.assertEqual(docs.man_text("^fg(lightblue)command"),
                         "DZENFMTBOLDTOKENcommandDZENFMTRESETTOKEN")

    def test_styled_example_keeps_rendered_input(self):
        styled = EXAMPLE.replace("^^fg(red)", "^fg(lightblue)^^fg^fg()(red)")
        readme, man, examples = docs.parse(styled)
        self.assertEqual(readme, docs.parse(EXAMPLE)[0])
        self.assertEqual(examples, docs.parse(EXAMPLE)[2])
        self.assertIn("DZENFMTBOLDTOKEN^fgDZENFMTRESETTOKEN(red)", man)
        self.assertEqual(docs.man_text("^^fg(lightblue)"), "^fg(lightblue)")

    def test_complete_command_styles(self):
        for argument in ("foo", "10;20", "red", "off", ""):
            source = ("^fg(lightblue)^^pa(^fg()^underline()" + argument +
                      "^underline(off)^fg(lightblue))^fg()") if argument else "^fg(lightblue)^^pa()^fg()"
            self.assertEqual(docs.input_text(source), f"^pa({argument})")
            self.assertEqual(docs.plain(source), f"^pa({argument})")
            expected = ("DZENFMTBOLDTOKEN^pa(DZENFMTRESETTOKEN"
                        "DZENFMTITALICTOKEN" + argument + "DZENFMTRESETTOKEN"
                        "DZENFMTBOLDTOKEN)DZENFMTRESETTOKEN") if argument else (
                            "DZENFMTBOLDTOKEN^pa()DZENFMTRESETTOKEN")
            self.assertEqual(docs.man_text(source), expected)

    def test_markdown_prose_commands(self):
        self.assertEqual(docs.markdown_prose("Use ^pa(foo), ^fg() and ^p(10;20)."),
                         "Use `^pa(foo)`, `^fg()` and `^p(10;20)`.")
        self.assertEqual(docs.markdown_prose("Use -underline N[,COLOR] or -geometry WxH+X+Y."),
                         "Use `-underline N[,COLOR]` or `-geometry WxH+X+Y`.")
        for text in ("Use `^pa(foo)`.", "Use ``a ` ^pa(foo)``."):
            self.assertEqual(docs.markdown_prose(text), text)

    def test_markdown_code_blocks(self):
        source = ("SYNOPSIS\n========\n\n    dzen2 -fg COLOR\n"
                  "    ^^pa(foo)\n\nUse ^^pa(foo).\n\n"
                  "```text\n^^pa(foo)\nExample: not-a-real-example\n```\n\n"
                  "~~~text\n^^fg(red)\n~~~\n")
        readme, _, _ = docs.parse(source)
        self.assertIn("    dzen2 -fg COLOR\n    ^pa(foo)\n", readme)
        self.assertIn("Use `^pa(foo)`.", readme)
        self.assertIn("```text\n^pa(foo)\nExample: not-a-real-example\n```", readme)
        self.assertIn("~~~text\n^fg(red)\n~~~", readme)

    def test_compound_cli_argument_styles(self):
        source = (ROOT / "README.dzen").read_text()
        for argument in ("N[,COLOR]", "WxH+X+Y", "l|c|r", "[SECONDS]"):
            self.assertIn("^underline()" + argument + "^underline(off)", source)

    def test_repository_source(self):
        readme, man, examples = docs.parse((ROOT / "README.dzen").read_text())
        self.assertGreaterEqual(len(examples), 6)
        self.assertNotIn("![", man)
        # The development section may name the directory, but never link a PNG.
        for ident in examples:
            self.assertIn(f"docs/screenshots/{ident}.png", readme)


if __name__ == "__main__":
    unittest.main()
