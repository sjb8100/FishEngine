from __future__ import print_function
import os
import clang.cindex
import json
import sys
from tool_helper import CamelCaseToReadable

debug = False

if sys.platform == 'darwin':
    libclang_path = R'/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/libclang.dylib'
    #libclang_path = R'/Users/yushroom/Downloads/llvm-3.9.1.src/build/lib/libclang.dylib'
else:
    libclang_path = R'D:\Program Files (x86)\LLVM\3.8.0\bin\libclang.dll'
src_dirs = ['../Engine/Source/Runtime']

clang.cindex.Config.set_library_file(libclang_path)
index = clang.cindex.Index.create()

namespaces = ('FishEngine', 'FishEditor')

classes = {}


skip_cursor_types = (clang.cindex.CursorKind.ENUM_DECL,
    clang.cindex.CursorKind.STRUCT_DECL,
    clang.cindex.CursorKind.FUNCTION_TEMPLATE,
    clang.cindex.CursorKind.UNEXPOSED_DECL,
    clang.cindex.CursorKind.TEMPLATE_REF
    )

skip_class_internal_type = (clang.cindex.CursorKind.UNEXPOSED_DECL,
    clang.cindex.CursorKind.CXX_ACCESS_SPEC_DECL,
    clang.cindex.CursorKind.TYPEDEF_DECL,
    clang.cindex.CursorKind.FUNCTION_TEMPLATE,
    clang.cindex.CursorKind.CONSTRUCTOR,
    clang.cindex.CursorKind.DESTRUCTOR,
    clang.cindex.CursorKind.ENUM_DECL,
    #clang.cindex.CursorKind.UNION_DECL, # TODO
    clang.cindex.CursorKind.VAR_DECL, # TODO, static var
    clang.cindex.CursorKind.CONVERSION_FUNCTION
    )

Serialization_type = (
    'float',
    'double',
    'int',
    'uint32_t',
    'bool',
    'std::string',
    )

all_attributes = (
    'DisallowMultipleComponent',
    'HideInInspector',
    'Serializable',
    'NonSerializable',
    'ExecuteInEditMode',
    'AddComponentMenu',
    'RequireComponent',
    )

class UnknownAttributeError(ValueError):
    pass

class ClassNameNotInjected(ValueError):
    pass

def internal_append_to_list_of_a_map(map, key, item):
    if key not in map:
        map[key] = [item]
    else:
        map[key].append(item)

def internal_get_annotation(node):
    for child in node.get_children():
        if child.kind == clang.cindex.CursorKind.ANNOTATE_ATTR:
            if child.spelling not in all_attributes:
                raise UnknownAttributeError()
            return child
    return None

def internal_is_derived_from_Component(classname):
    #print(classname)
    if classname == "Component":
        return True
    if 'parent' not in classes[classname]:
        return False
    return internal_is_derived_from_Component(classes[classname]['parent'])

def internal_is_derived_from_Object(classname):
    #print(classname)
    if classname == "Object":
        return True
    if 'parent' not in classes[classname]:
        return False
    return internal_is_derived_from_Object(classes[classname]['parent'])


def internal_parse_class(node):
    if not node.is_definition():
        return
    scope_prefix = '::'.join( node.type.spelling.split('::')[:-1] )
    #print('class', node.type.spelling)
    if scope_prefix not in namespaces:
        return
    class_name = node.spelling
    print('class', class_name)
    header_file = os.path.basename( node.location.file.name )

    if class_name in classes:
        ''' duplicate class?
        '''
        raise ValueError

    members = []
    # nonserializable = False
    classes[class_name] = {'scope_prefix': scope_prefix, 'header_file': header_file, 'is_unique': False, 'pretty_name': CamelCaseToReadable(node.spelling)}
    classname_injected = False
    static_classname_injected = False

    for child in node.get_children():
        # print('\t', child.kind, child.spelling, child.type.spelling)
        if child.kind == clang.cindex.CursorKind.CXX_BASE_SPECIFIER:
            base_class = next(child.get_children()).spelling
            base_class = base_class.split(' ')[-1].split('::')[-1]
            if base_class not in classes:
                ''' parent is NonSerializable
                '''
                print('\t', "base class", base_class, "is nonserializable")
                classes.pop(class_name)
                return
            classes[class_name]['parent'] = base_class

        elif child.kind == clang.cindex.CursorKind.CXX_METHOD:
            #print('\t', 'method', child.spelling)
            if child.spelling == "ClassName":
                classname_injected = True
            elif child.spelling == "StaticClassName":
                static_classname_injected = True
            pass

        # elif child.kind == clang.cindex.CursorKind.UNION_DECL:
        #     ''' eg. Vector4, Color...
        #     '''
        #     print('\t', 'union', child.spelling)
        #     for c in child.get_children():
        #         print('\t\t', c.spelling, c.kind)
        #         if c.kind == clang.cindex.CursorKind.STRUCT_DECL:
        #             for cc in c.get_children():
        #                 print('\t\t\t', cc.spelling, cc.kind, cc.type.spelling)
        #                 if cc.kind == clang.cindex.CursorKind.FIELD_DECL:
        #                     #annotation = internal_get_annotation(cc)
        #                     #if (annotation is not None) and (annotation.spelling == 'Serialize'):
        #                     #    internal_append_to_list_of_a_map(classes[node.spelling], 'member', {'name': cc.spelling, 'type': cc.type.spelling})
        #                     internal_append_to_list_of_a_map(classes[node.spelling], 'member', {'name': cc.spelling, 'type': cc.type.spelling})

        elif child.kind == clang.cindex.CursorKind.ANNOTATE_ATTR:
            #print('\t', child.type.spelling, child.spelling)
            if child.spelling not in all_attributes:
                raise UnknownAttributeError()
            if child.spelling == 'DisallowMultipleComponent':
                classes[class_name]['is_unique'] = True
            elif child.spelling == 'NonSerializable':
                print('\t', "class", class_name, "is nonserializable")
                classes.pop(class_name)
                return

        elif child.kind == clang.cindex.CursorKind.FIELD_DECL:
            # hack
            # libclang(python binding) not recognize stl class(std::list, std::vector, std::map...)(or forward declared type) and treats it as 'int'
            # when this happens, get actual type from raw tokens
            member_type = child.type.spelling
            maybe_error_type = (child.type.spelling == 'int')
            #maybe_error_type = True
            if maybe_error_type:
                #print('\tFIELD_DECL', child.spelling, child.type.spelling)
                toks = []
                end = False
                for tok in child.get_tokens():
                    if child.spelling == tok.spelling:
                        end = True
                        break
                    if tok not in ('mutable', 'volatile'):
                        toks.append(tok.spelling)
                if not end:
                    raise ValueError(toks)
                member_type = ''.join(toks)
                print('\tFIELD_DECL', child.spelling, child.type.spelling, member_type)
            NonSerializable = False
            HideInInspector = False
            for c in child.get_children():
                if (maybe_error_type):
                    print('\t\t', c.spelling, c.kind)
                if c.kind == clang.cindex.CursorKind.ANNOTATE_ATTR:
                    #print('\t', child.type.spelling, child.spelling)
                    if c.spelling not in all_attributes:
                        raise UnknownAttributeError()
                    elif c.spelling == 'NonSerializable':
                        NonSerializable = True
                    elif c.spelling == 'HideInInspector':
                        HideInInspector = True
            #if not NonSerializable:
            CamelCaseToReadable
            member = {'name': child.spelling, 'pretty_name': CamelCaseToReadable(child.spelling), 'type': member_type, 'NonSerializable': NonSerializable, 'HideInInspector': HideInInspector}
            #internal_append_to_list_of_a_map(classes[node.spelling], 'member', member)
            members.append(member)
            # else:
            #     print("Unkown type,", child.type.spelling)

        elif child.kind in skip_class_internal_type:
            pass
        else:
            pass

    if internal_is_derived_from_Object(class_name):
        if not (static_classname_injected and classname_injected):
            msg = r''' Did you forget to add InjectClassName to class defination?
                eg.
                class CameraController : public Script
                {
                public:
                    InjectClassName(CameraController)
                    ...
                }
            '''
            raise ClassNameNotInjected(msg)
    classes[class_name]['members'] = members

def internal_find_typerefs(node):
    global classes
    """ Find all references to the type named 'typename'
    """
    # if 'list' == node.spelling and node.is_definition():
    #     print(node.spelling, node.kind, node.type.spelling, node.location.file.name)
    #     for child in node.get_children():
    #         print(child.spelling, child.kind, child.type.spelling)
    #     return
    if (node.location.file is not None) and ('FishEngine' not in node.location.file.name):
        return
    if node.kind in skip_cursor_types:
        return

    #print node.spelling, str(node.kind)[str(node.kind).index('.')+1:], node.access_specifier
    #short_kind = str(node.kind)[str(node.kind).index('.')+1:]
    #short_kind = str(node.kind).split('.')
    #print('short kind', short_kind)

    if node.kind == clang.cindex.CursorKind.CLASS_DECL:
        internal_parse_class(node)

    for c in node.get_children():
        internal_find_typerefs(c)

def ExtractClasses(path):
    global classes
    #os.system(R'clang -x c++ -emit-ast -D__REFLECTION_PARSER__ -std=c++14 -I/Users/yushroom/program/graphics/FishEngine/Engine/ThirdParty/boost_1_61_0 -I/Users/yushroom/program/graphics/FishEngine/Engine/ThirdParty/glfw/include -I/Users/yushroom/program/graphics/FishEngine/Engine/ThirdParty/ -I/Users/yushroom/program/graphics/FishEngine/Engine/ThirdParty/PhysXSDK/Include temp/AllHeaders.hpp -o temp/AllHeaders.ast')
    tu = index.read("temp/AllHeaders.ast")
    internal_find_typerefs(tu.cursor)
    return classes

if __name__ == "__main__":
    classes = ExtractClasses("temp/AllHeaders.hpp")
    #print(classes)
    #print(classes['Object'])
    #print(classes['Color'])
    #print(json.dumps(classes, indent=4))
    print('Transform.m_children: ', [x for x in classes['Transform']['members'] if x['name'] == 'm_children'][0]['type'])
    #print(json.dumps(classes['Transform'], indent=4))
    #if debug:
    print('dump classes')
    #print(json.dumps(classes['Animator'], indent = 4))
    #print(json.dumps(classes['Rigidbody'], indent = 4))
    with open('/class.json', 'w') as f:
        f.write(json.dumps(classes, indent = 4))
