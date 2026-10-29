# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# Sphinx extension for the shirudo documentation: the page type and
# reader line under each title, the source and last change in the page
# footer, and console blocks whose prompts name the role a command runs in.

import os

from docutils import nodes
from pygments.lexer import RegexLexer, bygroups, default, using
from pygments.lexers.shell import BashLexer
from pygments.token import Generic, Whitespace


# "operator$ " runs on the operator host, "target# " as root on a target,
# "$ " and "# " anywhere. A backslash at the end continues the command.
class ConsoleLexer(RegexLexer):

    name = "shirudo console"
    aliases = ["console", "shell-session"]
    tokens = {
        "root": [
            (r"^(operator\$|target#|\$|#)( )",
             bygroups(Generic.Prompt, Whitespace), "command"),
            (r"[^\n]*\n", Generic.Output),
            (r"[^\n]+", Generic.Output),
        ],
        "command": [
            (r"[^\n]*\\\n", using(BashLexer)),
            (r"[^\n]*\n", using(BashLexer), "#pop"),
            (r"[^\n]+", using(BashLexer), "#pop"),
            default("#pop"),
        ],
    }


def meta_line(app, doctree, docname):
    meta = app.env.metadata.get(docname, {})
    parts = [meta.get(key) for key in ("page-type", "audience")]
    parts = [part for part in parts if part]
    if not parts:
        return
    for section in doctree.findall(nodes.section):
        title = section.next_node(nodes.title)
        if title is not None:
            line = nodes.paragraph("", " · ".join(parts), classes=["meta"])
            title.parent.insert(title.parent.index(title) + 1, line)
        return


CHANGES = {}


def last_changes():
    path = os.environ.get("DOCS_LASTCHANGE")
    changes = {}
    if not path or not os.path.exists(path):
        return changes
    with open(path, encoding="utf-8") as stream:
        for line in stream:
            fields = line.rstrip("\n").split("\t")
            if len(fields) == 3 and fields[0]:
                changes[fields[0]] = (fields[1], fields[2])
    return changes


def page_context(app, pagename, templatename, context, doctree):
    if doctree is None:
        return
    source = "Documentation/" + pagename + ".rst"
    context["source_file"] = source
    context["last_change"], context["last_change_date"] = \
        CHANGES.get(source, (None, None))
    context["edit_url"] = "%s/blob/%s/%s" % (
        context.get("repository"), context.get("branch"), source)


def first_paragraph(doctree, limit=300):
    for para in doctree.findall(nodes.paragraph):
        if "meta" in para.get("classes", []):
            continue
        text = " ".join(para.astext().split())
        if len(text) > limit:
            cut = text.rfind(". ", 0, limit)
            text = text[:cut + 1] if cut > 0 else text[:limit] + "..."
        return text
    return ""


def toctree_walk(env, docname):
    yield docname
    for child in env.toctree_includes.get(docname, []):
        yield from toctree_walk(env, child)


# llms.txt: an index of the plain-text pages for readers that are agents.
def write_llms(app, exception):
    if exception is not None or app.builder.name != "html":
        return
    env = app.env
    root = app.config.root_doc
    lines = ["# shirudo documentation", ""]
    intro = first_paragraph(env.get_doctree(root), 1000)
    if intro:
        lines += ["> " + intro, ""]
    lines += ["Every page below is plain text; the HTML version has the "
              "same path without _text/ and with .html.", ""]
    for section in env.toctree_includes.get(root, []):
        lines += ["## " + env.titles[section].astext(), ""]
        for docname in toctree_walk(env, section):
            title = env.titles[docname].astext()
            desc = first_paragraph(env.get_doctree(docname))
            entry = "- [%s](_text/%s.txt)" % (title, docname)
            lines.append(entry + (": " + desc if desc else ""))
        lines.append("")
    path = os.path.join(app.outdir, "llms.txt")
    with open(path, "w", encoding="utf-8") as stream:
        stream.write("\n".join(lines))


def setup(app):
    app.add_lexer("console", ConsoleLexer)
    app.add_lexer("shell-session", ConsoleLexer)
    CHANGES.update(last_changes())
    app.connect("doctree-resolved", meta_line)
    app.connect("html-page-context", page_context)
    app.connect("build-finished", write_llms)
    return {"parallel_read_safe": True, "parallel_write_safe": True}
