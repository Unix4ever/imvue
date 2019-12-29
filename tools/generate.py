"""
ImVue configuration generator
"""

import yaml
import os
import argparse
import re

from jinja2 import Environment, PackageLoader, select_autoescape


imgui_namespace = re.compile(r'namespace\s+ImGui\s+\{\s+(.+?)\n\}\s+\/\/ namespace ImGui', re.DOTALL)

pattern = re.compile(r'\s*IMGUI_API\s+(?P<return_type>(const)?[\w\*&]+)\s+(?P<func>[\w_]+)\((?P<parameters>.*?)\);\s*')
container_pattern = re.compile(r'^(Begin|End)(\w*)$')
dereference_pattern = re.compile(r'^(bool|int|unsigned int)\*$')

initialize_defaults = {
    'bool': 'true',
    'float': '0.0f',
    'int': '0',
    'ImGuiID': '0',
    'size_t': '0',
    'pointer': 'NULL',
    'flags': '0',
    'ImTextureID': 'NULL',
    'buf_size': '512', # default buffer size,
}

def create_arg_parser():
    """
    Create argument parser
    """
    parser = argparse.ArgumentParser(description="ImVue configuration generator")

    parser.add_argument('-f', '--files', nargs='+', help='Files list to scan')
    parser.add_argument('-c', '--config',
                        dest='config_file',
                        default='./config.yaml',
                        help='Yaml config file for generator')
    return parser


def cls_to_tag(name):
    s1 = re.sub('(.)([A-Z][a-z]+)', r'\1-\2', name)
    return re.sub('([a-z0-9])([A-Z])', r'\1-\2', s1).lower()


def gen_init_destroy(field_type, field_name, param_read, default=None):
    value = default
    t = field_type
    if 'flags' in field_type.lower():
        t = 'flags'
    if '*' in field_type:
        t = 'pointer'
    if '[' in field_name:
        t = field_type + '_array'
    if field_name == 'buf_size':
        t = field_name

    if default is None and t in initialize_defaults:
        value = initialize_defaults[t]

    initializer = None
    destructor = None
    if (t == 'pointer' or t == 'pointer_array') and 'void' not in field_type:
        if 'char' not in field_type:
            t = 'pointer_array'

        delete_function = 'ImGui::MemFree({})'.format(param_read)
        destructor = 'if({} != NULL) {}'.format(param_read, delete_function)
    elif 'array' in t:
        initializer = "%s{%s}" % (param_read, initialize_defaults.get(t.split('_')[0]))

    if value:
        initializer = "{}({})".format(param_read, value)

    return initializer, destructor


def parse_elements(filename, config):
    elements = {}

    blacklist = [re.compile(value) for value in config.get('blacklist', [])]
    rewrites = config.get('rewrites', {})
    rewrite_fields = config.get('rewrite_fields', {})
    rules = config.get('rules', {})
    elements_rules = config.get('elements_rules', {})

    with open(filename, "r") as f:
        imgui_api_match = re.search(imgui_namespace, f.read())
        if not imgui_api_match:
            return []

        (imgui_api, ) = imgui_api_match.groups()

        for match in re.finditer(pattern, imgui_api):
            d = match.groupdict()
            is_container = False
            cls = d['func']
            skip = False

            if '...' in d['parameters'] or 'getter' in d['parameters']:
                continue

            if cls in rewrites:
                cls = rewrites[cls]

            for p in blacklist:
                if p.match(cls):
                    skip = True
                    break

            if skip:
                continue

            m = container_pattern.match(d['func'])
            subtype = 'func'
            if m or d['func'] == 'CollapsingHeader':
                subtype, cls = m.groups()
                # handle special case for window begin/end
                if cls == '':
                    cls = 'Window'
                is_container = True


            if cls not in elements:
                elements[cls] = {
                    'functions': {
                    }
                }

            element = elements[cls]
            if subtype.lower() in element['functions']:
                continue

            element['tag'] = cls_to_tag(cls)
            element['cls'] = cls
            element['is_container'] = is_container
            if is_container and 'from_text' in element:
                del element['from_text']

            element_rules = elements_rules.get(cls, {})
            element['wrapped_by'] = element_rules.get('wrapped_by')

            required_fields = element_rules.get('required_fields', [])

            # special case for end popup
            if 'BeginPopup' in d['func']:
                element['functions']['end'] = {
                    'name': 'EndPopup',
                    'params': [],
                    'return_type': 'void'
                }

            return_type = d['return_type']
            params = []
            initializers = []
            init_buffers = []
            destructors = []

            if d['parameters']:

                params_string = d['parameters'].replace('&', '')
                for struct_default in re.finditer('(\w+\(.*?\))', params_string):
                    struct_default = struct_default.groups()[0]
                    params_string = params_string.replace(struct_default, struct_default.replace(',', ';'))

                for line in params_string.split(','):
                    line = line.replace(';', ',')
                    param = line.split('=')
                    definition = param[0].strip()
                    default = None

                    if len(param) > 1:
                        default = param[1].strip()
                        m = re.match(r'[\'"](.*?)[\'"]', default)
                        if m:
                            (val,) = m.groups()
                            default = "ImStrdup(\"{}\")".format(val)

                    parts = definition.split(' ')

                    field_type = ' '.join(parts[:-1])
                    param_type = field_type
                    field_type = field_type.replace('const', '').strip()
                    field_type.strip()

                    param_name = re.sub(r'\[\d*\]', '', parts[-1])
                    if param_name in rewrite_fields:
                        param_name = rewrite_fields[param_name]

                    attr_name = param_name.replace('_', '-')
                    param_read = param_name
                    field_name = parts[-1]
                    if field_name in rewrite_fields:
                        field_name = rewrite_fields[field_name]
                    array_size = re.match(r'\w*\[(\d+)\]', parts[-1])
                    if array_size:
                        (array_size,) = array_size.groups()

                    p = r'\[\]'
                    if re.search(p, field_name):
                        field_name = re.sub(p, '', field_name)
                        param_name = '&' + param_name

                    if dereference_pattern.match(field_type):
                        field_type = field_type.replace('*', '')
                        param_name = '&' + param_name
                        default = None

                    define = field_name != 'size'
                    params.append({
                        'default': default,
                        'attr_name': attr_name,
                        'array_size': array_size,
                        'field_type': field_type,
                        'param_type': param_type,
                        'field_name': field_name,
                        'param_name': param_name,
                        'param_read': param_read,
                        'required': param_read in required_fields,
                        'define': define
                    })

                    initializer, destructor = gen_init_destroy(field_type, field_name, param_read, default)
                    if initializer and define:
                        initializers.append(initializer)

                    if destructor:
                        destructors.append(destructor)

                    rule = rules.get(param_read)
                    if rule == 'from_text' and not element.get('is_container'):
                        if rule in element:
                            raise Exception('{} text source conflict with previously defined field {}'.format(field_name, element[rule]))
                        element[rule] = param_read

                    if param_read == 'buf':
                        init_buffers.append({
                            'field_type': field_type,
                            'buf_size': 'buf_size',
                            'field_name': field_name
                        })

            element['functions'][subtype.lower()] = {
                'name': d['func'],
                'params': params,
                'return_type': return_type
            }
            element['initializers'] = element.get('initializers', []) + initializers
            element['destructors'] = element.get('destructors', []) + destructors
            element['fields'] = element.get('fields', []) + params
            element['init_buffers'] = element.get('init_buffers', []) + init_buffers

    return [element for _, element in elements.items()]


def run():
    """
    Runs generator
    """
    arg_parser = create_arg_parser()
    options = arg_parser.parse_args()

    scan_files = (
        "../imgui/imgui.h",
    )

    if options.files:
       scan_files = options.files

    context = {'elements': []}

    config = {}
    with open(options.config_file, "r") as f:
        config = yaml.safe_load(f)

    for path in scan_files:
        context['elements'].extend(parse_elements(path, config))

    def find_element_by_class_name(cls):
        if not cls:
            return None

        vals = [element for element in context.get('elements', []) if element['cls'] == cls]
        if not vals:
            raise Exception('unknown element class {}'.format(cls))

        return vals[0]

    def get_test_value(element, field):
        element_rules = config.get('elements_rules', {}).get(element.get('cls'))
        values = element_rules.get('test_values', config.get('test_values', {}))
        res = values.get(field.get('param_read'))

        if res is None:
            res = config.get('test_values_by_type', {}).get(field.get('field_type'))

        return res

    def open_tag(element):
        if not element:
            return ''

        attributes = []
        text = ''
        for field in element.get('fields', []):
            if field.get('required'):
                if field['param_read'] == element.get('from_text'):
                    text = get_test_value(element, field)
                else:
                    attributes.append(
                        r"{}=\"{}\"".format(field['attr_name'], get_test_value(element, field))
                    )

        data = {
            'tag': element['tag'],
            'attributes': ' '.join(attributes),
            'text': text,
        }

        return "<{tag} {attributes}>{text}".format(**data)

    def close_tag(element):
        if not element:
            return ''

        return "</{}>".format(element['tag'])


    jinja_env = Environment(
        loader=PackageLoader('generate', 'templates'),
        autoescape=select_autoescape(['html', 'xml']),
    )

    jinja_env.filters['get_test_value'] = get_test_value
    jinja_env.filters['find_element_by_class_name'] = find_element_by_class_name
    jinja_env.filters['open_tag'] = open_tag
    jinja_env.filters['close_tag'] = close_tag

    template = jinja_env.get_template("imvue_generated.h.j2")

    with open("../src/imvue_generated.h", "w") as f:
        f.write(template.render(context))

    template = jinja_env.get_template("imvue_tests.cpp.j2")
    with open("../tests/unit/imvue_tests.cpp", "w") as f:
        f.write(template.render(context))


if __name__ == "__main__":
    run()
