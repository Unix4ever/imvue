
#include <gtest/gtest.h>
#include "imvue.h"
#include "imvue_errors.h"
#include "utils.h"

const char* simple =
"<template>"
  "<window name=\"test\">"
    "<button>button</button>"
  "</window>"
"</template>";

const char* badElements =
"<template>"
  "<no-such-element some=\"a\">hello</no-such-element>"
"</template>";

const char* badStructure =
"<button>I am the button</button>";

const char* badScript =
"<template><button>I am the button</button></template>"
"<script></script>";

/**
 * Parse and render simple document
 */
TEST(DocumentParser, TestSimple)
{
  ImVue::Document document;
  document.parse(simple);
  renderDocument(document);
}

/**
 * Check valid xml with not existing elements
 *
 * Expected to throw exception if found unknown elements
 */
TEST(DocumentParser, BadElements)
{

  ImVue::Document document;
  bool gotError = false;
  ASSERT_THROW(document.parse(badElements), ImVue::ElementError);
  renderDocument(document);
}

/**
 * Check valid xml with wrong structure
 *
 * Expected to raise DocumentError
 */
TEST(DocumentParser, BadStructure)
{
  ImVue::Document document;
  document.parse(badStructure);
  renderDocument(document);
}

/**
 * Script state is not initialized, should handle that
 *
 * Expected to raise DocumentError
 */
TEST(DocumentParser, BadScript)
{
  ImVue::Document document;
  document.parse(badScript);
  renderDocument(document);
}
