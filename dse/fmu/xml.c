// Copyright 2026 Robert Bosch GmbH
//
// SPDX-License-Identifier: Apache-2.0

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <libxml/xpath.h>
#include <dse/fmu/xml.h>


void* xml_object_enumerator(xmlXPathContextPtr ctx, const char* xpath,
    uint32_t* index, XmlObjectGenerator generator, void* userdata)
{
    assert(ctx);
    assert(xpath);
    assert(index);
    assert(generator);

    xmlXPathObjectPtr obj = xmlXPathEvalExpression(BAD_CAST xpath, ctx);
    if (obj == NULL || obj->nodesetval == NULL ||
        obj->nodesetval->nodeNr == 0) {
        if (obj) xmlXPathFreeObject(obj);
        return NULL;
    }
    if ((int)*index >= obj->nodesetval->nodeNr) {
        xmlXPathFreeObject(obj);
        return NULL;
    }

    /* Generate the object, and return. */
    void* o = generator(obj->nodesetval->nodeTab[*index], userdata);
    *index = *index + 1;
    xmlXPathFreeObject(obj);
    return o; /* Caller to free. */
}


void xml_load_object(xmlXPathContextPtr ctx, void* object,
    const XmlFieldSpec* spec, size_t count)
{
    uint8_t* o = (uint8_t*)object;
    for (size_t i = 0; i < count; i++) {
        const XmlFieldSpec* s = &spec[i];
        xmlXPathObjectPtr res = xmlXPathEvalExpression(BAD_CAST s->xpath, ctx);
        if (res == NULL || res->nodesetval == NULL ||
            res->nodesetval->nodeNr == 0) {
            if (res) xmlXPathFreeObject(res);
            continue;
        }
        xmlNodePtr node = res->nodesetval->nodeTab[0];
        xmlChar*   raw = s->attr ? xmlGetProp(node, BAD_CAST s->attr)
                                 : xmlNodeGetContent(node);
        xmlXPathFreeObject(res);
        if (raw == NULL) continue;

        if (s->map) {
            for (const XmlFieldMapSpec* m = s->map; m && m->key; m++) {
                if (strcmp((char*)raw, m->key) == 0) {
                    *(int*)(o + s->offset) = m->val;
                    break;
                }
            }
        } else {
            switch (s->type) {
            case XmlFieldTypeU8:
                *(uint8_t*)(o + s->offset) =
                    (uint8_t)strtol((char*)raw, NULL, 10);
                break;
            case XmlFieldTypeU16:
                *(uint16_t*)(o + s->offset) =
                    (uint16_t)strtol((char*)raw, NULL, 10);
                break;
            case XmlFieldTypeU32:
                *(uint32_t*)(o + s->offset) =
                    (uint32_t)strtoul((char*)raw, NULL, 10);
                break;
            case XmlFieldTypeD:
                *(double*)(o + s->offset) = strtod((char*)raw, NULL);
                break;
            case XmlFieldTypeB:
                *(bool*)(o + s->offset) = (strcmp((char*)raw, "true") == 0 ||
                                           strcmp((char*)raw, "1") == 0);
                break;
            case XmlFieldTypeS:
                *(const char**)(o + s->offset) = strdup((char*)raw);
                break;
            case XmlFieldTypeI:
                *(int*)(o + s->offset) = (int)strtol((char*)raw, NULL, 10);
                break;
            default:
                break;
            }
        }
        xmlFree(raw);
    }
}
