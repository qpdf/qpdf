from collections import defaultdict
from operator import itemgetter
import re

from sphinx import addnodes
from sphinx.directives import ObjectDescription
from sphinx.domains import Domain, Index
from sphinx.roles import XRefRole
from sphinx.util.nodes import make_refnode

# Reference:
#   https://www.sphinx-doc.org/en/master/development/tutorials/todo.html
#   https://www.sphinx-doc.org/en/master/development/tutorials/recipe.html


class OptionDirective(ObjectDescription):
    has_content = True

    def handle_signature(self, sig, signode):
        signode += addnodes.desc_name(text=sig)
        return sig

    def add_target_and_index(self, name_cls, sig, signode):
        m = re.match(r'^--([^\[= ]+)', sig)
        if not m:
            raise Exception('option must start with --')
        option_name = m.group(1)
        signode['ids'].append(f'option-{option_name}')
        qpdf = self.env.get_domain('qpdf')
        qpdf.add_option(sig, option_name)


class OptionIndex(Index):
    name = 'options'
    localname = 'qpdf Command-line Options'
    shortname = 'Options'

    def generate(self, docnames=None):
        content = defaultdict(list)
        options = self.domain.get_objects()
        options = sorted(options, key=itemgetter(0))

        # name, subtype, docname, anchor, extra, qualifier, description
        for name, display_name, typ, docname, anchor, _ in options:
            m = re.match(r'^(--([^\[= ]+))', display_name)
            if not m:
                raise Exception(
                    'OptionIndex.generate: display name not as expected')
            content[m.group(2)[0].lower()].append(
                (m.group(1), 0, docname, anchor, '', '', typ))

        content = sorted(content.items())
        return content, True


class QpdfDomain(Domain):
    name = 'qpdf'
    label = 'qpdf documentation domain'
    roles = {
        'ref': XRefRole()
    }
    directives = {
        'option': OptionDirective,
    }
    indices = {
        OptionIndex,
    }
    initial_data = {
        'options': [],  # object list
    }

    def get_full_qualified_name(self, node):
        return '{}.{}'.format('option', node.arguments[0])

    def get_objects(self):
        for obj in self.data['options']:
            yield(obj)

    def resolve_xref(self, env, from_doc_name, builder, typ, target, node,
                     contnode):
        match = [(docname, anchor)
                 for name, sig, typ, docname, anchor, priority
                 in self.get_objects() if name == f'option.{target[2:]}']

        if len(match) > 0:
            to_doc_name = match[0][0]
            match_target = match[0][1]
            return make_refnode(builder, from_doc_name, to_doc_name,
                                match_target, contnode, match_target)
        else:
            raise Exception(f'invalid option xref ({target})')

    def add_option(self, signature, option_name):
        if self.env.docname != 'cli':
            raise Exception(
                'qpdf:option directives don\'t work outside of cli.rst')

        name = f'option.{option_name}'
        anchor = f'option-{option_name}'

        # name, display_name, type, docname, anchor, priority
        self.data['options'].append(
            (name, signature, '', self.env.docname, anchor, 0))

    def purge_options(self, docname):
        self.data['options'] = list([
            x for x in self.data['options']
            if x[3] != docname
        ])


def purge_options(app, env, docname):
    option = env.get_domain('qpdf')
    option.purge_options(docname)


def setup(app):
    app.add_domain(QpdfDomain)
    app.connect('env-purge-doc', purge_options)

    return {
        'version': '0.1',
        'parallel_read_safe': True,
        'parallel_write_safe': True,
    }
