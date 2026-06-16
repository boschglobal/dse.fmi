// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#ifndef DSE_FMU_XML_H_
#define DSE_FMU_XML_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <libxml/xpath.h>
#include <dse/fmu/fmu.h>

#ifndef DLL_PRIVATE
#define DLL_PRIVATE __attribute__((visibility("hidden")))
#endif

typedef enum XmlFieldType {
    XmlFieldTypeNONE = 0,
    XmlFieldTypeU8,   // uint8_t     — parsed from node value or attribute
    XmlFieldTypeU16,  // uint16_t    — parsed from node value or attribute
    XmlFieldTypeU32,  // uint32_t    — parsed from node value or attribute
    XmlFieldTypeD,    // double      — parsed from node value or attribute
    XmlFieldTypeB,    // bool        — "true"/"1" -> true, else false
    XmlFieldTypeS,    // const char* — strdup'd from node value or attribute
    XmlFieldTypeI,    // int         — parsed from node value or attribute
} XmlFieldType;

typedef struct XmlFieldMapSpec {
    const char* key;
    int         val;
} XmlFieldMapSpec;

typedef struct XmlFieldSpec {
    XmlFieldType           type;
    const char*            xpath;   // XPath to evaluate against the context
    size_t                 offset;  // offsetof() into the target struct
    const char*            attr;    // attribute name, or NULL for text content
    const XmlFieldMapSpec* map;     // optional key->val mapping (NTL)
} XmlFieldSpec;

typedef void* (*XmlObjectGenerator)(xmlNodePtr node, void* userdata);

DLL_PRIVATE void  xml_load_object(xmlXPathContextPtr ctx, void* object,
     const XmlFieldSpec* spec, size_t count);
DLL_PRIVATE void* xml_object_enumerator(xmlXPathContextPtr ctx,
    const char* xpath, uint32_t* index, XmlObjectGenerator generator,
    void* userdata);

#endif  // DSE_FMU_XML_H_
