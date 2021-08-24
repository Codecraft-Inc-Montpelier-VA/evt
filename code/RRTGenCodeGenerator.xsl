<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet version="2.0" xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0"
            xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0"
            xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0"
            xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0"
            xmlns:xs="http://www.w3.org/2001/XMLSchema"
            xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
            xmlns:ipo="http://www.altova.com/IPO"
            xmlns:fn="http://www.w3.org/2006/xpath-functions"
            xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">

    <xsl:output method="text" encoding="us-ascii" />

    <xsl:variable name="COPYRIGHT">
        <xsl:text>Copyright (c) 2007-2015 Codecraft, Inc.  All rights reserved.</xsl:text>
    </xsl:variable>

    <xsl:variable name="MAX_NAME_LENGTH" select="50"/>

    <xsl:variable name="INDENT1" select="$INDENT_BLOCK"/>
    <xsl:variable name="INDENT2" select="concat($INDENT_BLOCK, $INDENT1)"/>
    <xsl:variable name="INDENT3" select="concat($INDENT_BLOCK, $INDENT2)"/>
    <xsl:variable name="INDENT4" select="concat($INDENT_BLOCK, $INDENT3)"/>
    <xsl:variable name="INDENT5" select="concat($INDENT_BLOCK, $INDENT4)"/>
    <xsl:variable name="CR">
        <xsl:text>&#xA;</xsl:text>
    </xsl:variable>
    <xsl:variable name="CR_INDENT1" select="concat($CR, $INDENT1)"/>
    <xsl:variable name="CR_INDENT2" select="concat($CR, $INDENT2)"/>
    <xsl:variable name="CR_INDENT3" select="concat($CR, $INDENT3)"/>
    <xsl:variable name="CR_INDENT4" select="concat($CR, $INDENT4)"/>
    <xsl:variable name="CR_INDENT5" select="concat($CR, $INDENT5)"/>
    <xsl:variable name="RIGHT_ARROW" select="' ---)) '"/>
    <xsl:variable name="MODEL_ABBREVIATION" select="'EVT'"/>
    <xsl:variable name="MODEL_NAME" select="concat($MODEL_ABBREVIATION, 'Model')"/>
    <xsl:variable name="INSERT_CPP_NAME" select="concat($MODEL_NAME, '_Insert.cpp')" as="xs:string"/>
    <xsl:variable name="INSERT_H_NAME" select="concat($MODEL_NAME, '_Insert.h')" as="xs:string"/>
    <xsl:variable name="COMMENT_START" select="concat('&lt;', '!', '-- ')"/>

    <xsl:variable name="VERSION" select="1.1"/>
    <xsl:variable name="MODEL_VERSION" select="0.61"/>

    <xsl:variable name="INDENT_BLOCK">
        <xsl:text>   </xsl:text>
    </xsl:variable>

    <xsl:variable name="ALL_TRANSITION_STYLES" select="//style:style[@style:parent-style-name='_5f_TRANSITION']" as="element()*"/>

    <xsl:variable name="ALL_STATES" as="xs:string*">
        <xsl:for-each select="//draw:page[@draw:name!='Title Page']">
            <xsl:for-each select="draw:custom-shape">
                <xsl:value-of select="text:p"/>
            </xsl:for-each>
        </xsl:for-each>
        <xsl:value-of select="'Start'"/> <!-- draw:ellipse doesn't have text:p -->
    </xsl:variable>
    <xsl:variable name="ALL_UNIQUE_SORTED_STATES" as="xs:string*">
        <xsl:perform-sort select="distinct-values($ALL_STATES)">
            <xsl:sort select="."/>
        </xsl:perform-sort>
    </xsl:variable>

    <xsl:variable name="ALL_EVENTS" as="xs:string*">
        <xsl:for-each select="//draw:page[@draw:name!='Title Page']">
            <xsl:apply-templates select="draw:connector" mode="TRANSITION_NAME" />
        </xsl:for-each>
    </xsl:variable>
    <xsl:variable name="ALL_UNIQUE_SORTED_EVENTS" as="xs:string*">
        <xsl:perform-sort select="distinct-values($ALL_EVENTS)">
            <xsl:sort select="."/>
        </xsl:perform-sort>
    </xsl:variable>

    <xsl:strip-space elements="*"/>

    <xsl:template match="/">
        <xsl:result-document href="{$INSERT_CPP_NAME}">
          <xsl:call-template name="GENERATE_INSERT_CPP"/>
        </xsl:result-document>
        <xsl:result-document href="{$INSERT_H_NAME}">
          <xsl:call-template name="GENERATE_INSERT_H"/>
        </xsl:result-document>
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- ___________________________________________  GENERATE_INSERT_CPP  ___________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="GENERATE_INSERT_CPP">
        <xsl:message>  Document: <xsl:value-of select="$INSERT_CPP_NAME"/> </xsl:message>
        <xsl:call-template name="CPP_FILE_HEADER">
            <xsl:with-param name="GeneratorVersion" select="$VERSION"/>
            <xsl:with-param name="ModelVersion" select="$MODEL_VERSION"/>
            <xsl:with-param name="ModelFilename" select="//text:file-name"/>
            <xsl:with-param name="ModelName" select="$MODEL_NAME"/>
            <xsl:with-param name="ModelAbbreviation" select="$MODEL_ABBREVIATION"/>
            <xsl:with-param name="Copyright" select="$COPYRIGHT"/>
        </xsl:call-template>

        <xsl:for-each select="//draw:page[@draw:name!='Title Page']">
            <xsl:message>      Page: <xsl:value-of select="@draw:name"/> </xsl:message>
            <xsl:call-template name="SLIDE_PREAMBLE"> <xsl:with-param name="PageName" select="@draw:name"/> </xsl:call-template>
            <xsl:variable name="EVENTS" as="xs:string*">
                <xsl:apply-templates select="draw:connector" mode="TRANSITION_NAME" />
            </xsl:variable>
            <xsl:variable name="UNIQUE_SORTED_EVENTS" as="xs:string*">
                <xsl:perform-sort select="distinct-values($EVENTS)">
                    <xsl:sort select="."/>
                </xsl:perform-sort>
            </xsl:variable>
            <xsl:call-template name="STATE_TRANSITION_TABLE"> <xsl:with-param name="UniqueEvents" select="$UNIQUE_SORTED_EVENTS"/> </xsl:call-template>
            <xsl:call-template name="SLIDE_POSTAMBLE"/>
        </xsl:for-each>

        <xsl:call-template name="MODEL_COMPONENT_INSTANCES"/>
        <xsl:call-template name="EVENT_TEXT"> <xsl:with-param name="AllUniqueEvents" select="$ALL_UNIQUE_SORTED_EVENTS"/> </xsl:call-template>
        <xsl:call-template name="STATE_TEXT"> <xsl:with-param name="AllUniqueStates" select="$ALL_UNIQUE_SORTED_STATES"/> </xsl:call-template>
#pragma GCC diagnostic pop
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- ____________________________________________  GENERATE_INSERT_H  ____________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="GENERATE_INSERT_H">
        <xsl:message>  Document: <xsl:value-of select="$INSERT_H_NAME"/> </xsl:message>
        <xsl:call-template name="H_FILE_HEADER">
            <xsl:with-param name="GeneratorVersion" select="$VERSION"/>
            <xsl:with-param name="ModelVersion" select="$MODEL_VERSION"/>
            <xsl:with-param name="ModelFilename" select="//text:file-name"/>
            <xsl:with-param name="ModelName" select="$MODEL_NAME"/>
            <xsl:with-param name="ModelAbbreviation" select="$MODEL_ABBREVIATION"/>
            <xsl:with-param name="Copyright" select="$COPYRIGHT"/>
        </xsl:call-template>
        <xsl:call-template name="STATE_ENUMS"> <xsl:with-param name="AllUniqueStates" select="$ALL_UNIQUE_SORTED_STATES"/> </xsl:call-template>
        <xsl:call-template name="EVENT_ENUMS"> <xsl:with-param name="AllUniqueEvents" select="$ALL_UNIQUE_SORTED_EVENTS"/> </xsl:call-template>
        <xsl:call-template name="MODEL_COMPONENTS"/>
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- ______________________________________  STATE_TRANSITION_TABLE (call)  ______________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="STATE_TRANSITION_TABLE">
        <xsl:param name="UniqueEvents"/>
        <xsl:apply-templates select="." mode="ITERATE_OVER_TRANSITIONS" >
            <xsl:with-param name="thePage"  select="." />
            <xsl:with-param name="PageName"  select="@draw:name" />
            <xsl:with-param name="Events"  select="$UniqueEvents" />
        </xsl:apply-templates>
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- ______________________________________  STATE_TRANSITION_TABLE (match)  _____________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template match="draw:page" mode="ITERATE_OVER_TRANSITIONS">
        <xsl:param name="thePage"/>
        <xsl:param name="PageName"/>
        <xsl:param name="Events"/>
   switch( theEvent )
   {    <xsl:for-each select="$Events">
            <xsl:variable name="CLEAN_TRANSITION_NAME">
                <xsl:value-of select="."/>
            </xsl:variable>
      case <xsl:value-of select="$CLEAN_TRANSITION_NAME"/>:
         switch( currentState )
         {  <xsl:for-each select="$thePage/draw:connector">
                <xsl:variable name="CLEAN_CANDIDATE_TRANSITION_NAME">
                    <xsl:apply-templates select="." mode="TRANSITION_NAME"/>
                </xsl:variable>
                <xsl:variable name="PARAMETERS">
                    <xsl:apply-templates select="." mode="TRANSITION_PARAMETERS"/>
                </xsl:variable>
                <xsl:variable name="STATE_NAME">
			        <xsl:choose>
			            <xsl:when test="empty($thePage/draw:custom-shape[@draw:id = current()/@draw:start-shape])">
			                <xsl:value-of select="'Start'"/>
			            </xsl:when>
			            <xsl:otherwise>
			                <xsl:apply-templates select="$thePage/draw:custom-shape[@draw:id = current()/@draw:start-shape]" mode="STATE_NAME"/>
			            </xsl:otherwise>
			        </xsl:choose>
                </xsl:variable>
                <xsl:if test="../@draw:name = $PageName">
                    <xsl:if test="($CLEAN_CANDIDATE_TRANSITION_NAME = $CLEAN_TRANSITION_NAME) and (string-length($STATE_NAME)>0)">
            case <xsl:value-of select="$STATE_NAME"/>:
               // [<xsl:apply-templates select="." mode="FUNCTION_ID"/>]
               <!--  --><xsl:value-of select="$MODEL_NAME"/>_<xsl:apply-templates select="." mode="FUNCTION_ID"/>(<xsl:text/>
                        <xsl:value-of select="if ($PARAMETERS ne '') then ' parm1, parm2 ' else ''"/>);
               nextState = <xsl:apply-templates select="$thePage/draw:custom-shape[@draw:id = current()/@draw:end-shape]" mode="STATE_NAME"/>;
               break;
                    </xsl:if>
                </xsl:if>
            </xsl:for-each>
         }
         break;
        </xsl:for-each>
   }
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- _____________________________________________  CPP_FILE_HEADER  _____________________________________________ -->
    <!-- _____________________________________________________________________________________________________________  -->

    <xsl:template name="CPP_FILE_HEADER">
        <xsl:param name="GeneratorVersion"/>
        <xsl:param name="ModelVersion"/>
        <xsl:param name="ModelFilename"/>
        <xsl:param name="ModelName"/>
        <xsl:param name="ModelAbbreviation"/>
        <xsl:param name="Copyright"/>
/**************************** DO NOT EDIT THIS FILE ****************************
 *
 *  Filename:  <xsl:value-of select="$ModelAbbreviation"/>Model_Insert.cpp
 *
 *  This file is one of two automatically-generated files for the RRTGen
 *  <xsl:value-of select="$ModelName"/>.  These automatically-generated files should not be
 *  edited; rather, make appropriate updates to the source Draw file and
 *  regenerate the files.
 *
 *       Created by:  RRTGenCodeGenerator.xsl, Version <xsl:value-of select="$GeneratorVersion"/>
 *  From model file:  <xsl:value-of select="$ModelFilename"/>
 *    Model version:  <xsl:value-of select="$ModelVersion"/>
 *          On date:  <xsl:value-of select="format-dateTime(current-dateTime(), '[MNn] [Do], [Y], at [h].[m01] [P]')"/>
 *
 *        <xsl:value-of select="$Copyright"/>
 */

#include &lt;string.h&gt;
#include "<xsl:value-of select='$ModelAbbreviation'/>TestConstants.h"
#include "<xsl:value-of select='$ModelAbbreviation'/>Model_Insert.h"

// Incorporate the RRTGen framework.
#include "../../RRTGen/code/RRTGen.cpp"
#include "../../RRTGen/code/RRandom.cpp"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wswitch"

unsigned long RRandom::PrevValue[ MAX_RRANDOM_IDS ];
int RRandom::Seed = 0;

extern RepeatableRandomTest rrt;       // the test
extern FSM                  *sender;</xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- ______________________________________________  H_FILE_HEADER  ______________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="H_FILE_HEADER">
        <xsl:param name="GeneratorVersion"/>
        <xsl:param name="ModelVersion"/>
        <xsl:param name="ModelFilename"/>
        <xsl:param name="ModelName"/>
        <xsl:param name="ModelAbbreviation"/>
        <xsl:param name="Copyright"/>
/**************************** DO NOT EDIT THIS FILE ****************************
 *
 *  Filename:  <xsl:value-of select="$ModelAbbreviation"/>Model_Insert.h
 *
 *  This file is one of two automatically-generated files for the RRTGen
 *  <xsl:value-of select="$ModelName"/>.  These automatically-generated files should not be
 *  edited; rather, make appropriate updates to the source Draw file and
 *  regenerate the files.
 *
 *       Created by:  RRTGenCodeGenerator.xsl, Version <xsl:value-of select="$GeneratorVersion"/>
 *  From model file:  <xsl:value-of select="$ModelFilename"/>
 *    Model version:  <xsl:value-of select="$ModelVersion"/>
 *          On date:  <xsl:value-of select="format-dateTime(current-dateTime(), '[MNn] [Do], [Y], at [h].[m01] [P]')"/>
 *
 *        <xsl:value-of select="$Copyright"/>
 */

 #pragma once

    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- ______________________________________________  SLIDE_PREAMBLE  _____________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="SLIDE_PREAMBLE">
        <xsl:param name="PageName"/>

void <xsl:value-of select="$PageName"/>_Model::Update( event theEvent, char parm1, char parm2 )
{
   #if defined( SHOW_EVENT_PROCESSING )
   char str[ 120 ]; // ample
   sprintf( str, "Event '%s(%c,%c)' @ state '%s' in %s (%i).",
            EventText( theEvent ), parm1, parm2,
            StateText( currentState ), name, theInstance );
   rrt.Log( str );
   //cout &lt;&lt; str &lt;&lt; endl;
   #endif // defined( SHOW_EVENT_PROCESSING )

   state nextState = currentState;
   sender = this;
</xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- ______________________________________________  SLIDE_POSTAMBLE  ____________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="SLIDE_POSTAMBLE">
   currentState = nextState;

   #if defined( SHOW_EVENT_PROCESSING )
   sprintf( str, "New state is '%s'.", StateText( currentState ) );
   rrt.Log( str );
   cout &lt;&lt; str &lt;&lt; endl;
   #endif // defined( SHOW_EVENT_PROCESSING )
} </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- _________________________________________  MODEL_COMPONENT_INSTANCES  _______________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="MODEL_COMPONENT_INSTANCES">
<!--
// Model component instances.&#xA;<xsl:text/>
    <xsl:for-each select="//draw:page[@draw:name!='Title Page']">
<xsl:value-of select="string-join((@draw:name, '_Model', ' ', for $sp in 1 to 39 - string-length(@draw:name) return ' ', @draw:name), '')"/>;&#xA;<xsl:text/>
    </xsl:for-each>
 -->
FSM *theModel[ MAX_NUMBER_OF_MODEL_INSTANCES] = { 0 };

int currentNumberOfModelInstances = 0;

FSM **pModelComponentArray = (FSM **)theModel; // initialized defensively
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- _________________________________________________  EVENT_TEXT  ______________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="EVENT_TEXT">
        <xsl:param name="AllUniqueEvents"/>
////////////////////////////////////////////////////////////////////////////////
//
// EventText
//
// This utility returns a text description for a given event.
//

char *EventText( event theEvent )
{
   static char str[ 80 ]; // ample
   sprintf( str, "&lt;unknown event (%i)&gt;", theEvent );
   const char *theEventStr = str;

   switch( theEvent )
   {    <xsl:for-each select="$AllUniqueEvents">
            <xsl:variable name="CLEAN_TRANSITION_NAME">
                <xsl:value-of select="."/>
            </xsl:variable>
      case <xsl:value-of select="$CLEAN_TRANSITION_NAME"/>:
         theEventStr = "<xsl:value-of select="$CLEAN_TRANSITION_NAME"/>";
         break;
	    </xsl:for-each>
   }
   return (char *)theEventStr;
}
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- _________________________________________________  STATE_TEXT  ______________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="STATE_TEXT">
        <xsl:param name="AllUniqueStates"/>
////////////////////////////////////////////////////////////////////////////////
//
// StateText
//
// This utility returns a text description for a given state.
//

char *StateText( state theState )
{
   const char *theStateStr = "&lt;unknown state&gt;";

   switch( theState )
   {    <xsl:for-each select="$AllUniqueStates">
            <xsl:variable name="STATE_NAME">
                <xsl:value-of select="."/>
            </xsl:variable>
      case <xsl:value-of select="$STATE_NAME"/>:
         theStateStr = "<xsl:value-of select="$STATE_NAME"/>";
         break;
	    </xsl:for-each>
   }
   return (char *)theStateStr;
}
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- _________________________________________________  STATE_ENUMS  _____________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="STATE_ENUMS">
        <xsl:param name="AllUniqueStates"/>
enum state
{
   <xsl:for-each select="$AllUniqueStates">
   <xsl:value-of select="."/>,
   </xsl:for-each>
   STATE_COUNT
};
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- _________________________________________________  EVENT_ENUMS  _____________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="EVENT_ENUMS">
        <xsl:param name="AllUniqueEvents"/>
enum event
{
   <xsl:for-each select="$AllUniqueEvents">
   <xsl:value-of select="."/>,
   </xsl:for-each>
   EVENT_COUNT
};
    </xsl:template>

    <!-- _____________________________________________________________________________________________________________ -->
    <!-- ______________________________________________  MODEL_COMPONENTS  ___________________________________________ -->
    <!-- _____________________________________________________________________________________________________________ -->

    <xsl:template name="MODEL_COMPONENTS">
// The Fifo and FSM classes from the RRTGen framework are included here.
#include "RRTGenClasses.h"

// Model components.<xsl:text/>
        <xsl:for-each select="//draw:page[@draw:name!='Title Page']">
            <xsl:variable name="COMPONENT" as="xs:string" select="@draw:name"/>
            <xsl:variable name="COMPONENT_MODEL" as="xs:string" select="string-join((@draw:name, '_Model'), '')"/>
class <xsl:value-of select="$COMPONENT_MODEL"/>;<xsl:text/>
        </xsl:for-each>
        <xsl:text>&#xA;</xsl:text>
        <xsl:for-each select="//draw:page[@draw:name!='Title Page']">
            <xsl:variable name="COMPONENT" as="xs:string" select="@draw:name"/>
            <xsl:variable name="COMPONENT_MODEL" as="xs:string" select="string-join((@draw:name, '_Model'), '')"/>
            <xsl:variable name="INDENT" as="xs:string" select="string-join(for $i in 1 to string-length($COMPONENT_MODEL) + 2 return ' ', '')"/>
class <xsl:value-of select="$COMPONENT_MODEL"/> : public FSM   // <xsl:value-of select="$COMPONENT"/>
{<xsl:text/>
   			<xsl:variable name="MEMBER_VARIABLES" as="xs:string*">
       			<xsl:apply-templates select="draw:rect[contains(string(),'Local Variables')]" mode="DATA_MEMBERS"/>
   			</xsl:variable>
            <xsl:for-each select="$MEMBER_VARIABLES">
                <xsl:variable name="M_V_EXPLODED" select="tokenize(., '\s+')"/>
                <xsl:variable name="M_V_TYPE" select="subsequence($M_V_EXPLODED, 3, index-of($M_V_EXPLODED, '=') - 3)"/>
   <!--      --><xsl:value-of select="concat($CR_INDENT1, concat(string-join(($M_V_TYPE, $M_V_EXPLODED[1]), ' '), ';'))"/>
            </xsl:for-each>

 public:
   <xsl:value-of select="$COMPONENT_MODEL"/>( int instanceNumber = 1,
   <xsl:value-of select="$INDENT"/>const char *modelName = "<xsl:value-of select="$COMPONENT"/>",
   <xsl:value-of select="$INDENT"/>state initialState = Start )
   : FSM( instanceNumber, initialState, modelName )<xsl:text/>
            <xsl:for-each select="$MEMBER_VARIABLES">
                <xsl:variable name="M_V_EXPLODED" select="tokenize(., '\s+')"/>
                <xsl:variable name="M_V_VALUE" select="subsequence($M_V_EXPLODED, index-of($M_V_EXPLODED, '=') + 1, 1)"/>
   <!--      --><xsl:value-of select="string-join((',', $CR_INDENT1, '  ', $M_V_EXPLODED[1], '( ', $M_V_VALUE, ' )'), '')"/>
            </xsl:for-each>
   {}
            <xsl:for-each select="distinct-values(draw:connector)">
                <xsl:sort select="substring-after(., '[')"/>
                <xsl:variable name="FUNCTION_ID" as="xs:string">
                    <xsl:value-of select="substring-before(substring-after(., '['), ']')"/>
                </xsl:variable>
                <xsl:variable name="DRAW_PARAMETERS">
                    <xsl:value-of select="substring-before(substring-after(., '('), ']')"/>
                </xsl:variable>
                <xsl:variable name="PARAMETERS">
			        <xsl:choose>
			            <xsl:when test="string-length($DRAW_PARAMETERS) eq 0">
			                <xsl:value-of select="'void'"/>
			            </xsl:when>
			            <xsl:otherwise>
			                <xsl:value-of select="'char parm1, char parm2'"/>
			            </xsl:otherwise>
			        </xsl:choose>
			    </xsl:variable>
   void EVTModel_<xsl:value-of select="$FUNCTION_ID"/>( <xsl:value-of select="$PARAMETERS"/> );<xsl:text/>
            </xsl:for-each>
   			<xsl:variable name="MEMBER_METHODS" as="xs:string*">
       			<xsl:apply-templates select="draw:rect[contains(string(),'Local Variables')]" mode="METHODS"/>
   			</xsl:variable>
	        <xsl:choose>
	            <xsl:when test="count($MEMBER_METHODS) != 0">
	                <xsl:value-of select="'&#xA;'"/>
	            </xsl:when>
	        </xsl:choose>
            <xsl:for-each select="$MEMBER_METHODS">
                <xsl:variable name="M_M_EXPLODED" select="tokenize(., '\s+')"/>
                <xsl:variable name="M_M_SIGNATURE">
                    <xsl:value-of select="substring-before(substring-after($M_M_EXPLODED[1], '('), ')')"/>
                </xsl:variable>
                <xsl:variable name="M_M_PARMS_LIST" select="for $x in tokenize($M_M_SIGNATURE, ',') return concat(substring-after($x, ':'), ' ', substring-before($x, ':'))"/>
                <xsl:variable name="M_M_PARMS">
                	<xsl:choose>
                		<xsl:when test="count($M_M_PARMS_LIST) != 0">
                			<xsl:value-of select="string-join($M_M_PARMS_LIST, ', ')"/>
                		</xsl:when>
                		<xsl:otherwise>
                			<xsl:value-of select="$M_M_PARMS_LIST"/>
                		</xsl:otherwise>
                	</xsl:choose>
                </xsl:variable>
                <xsl:variable name="M_M_CALL">
			        <xsl:choose>
			            <xsl:when test="string-length($M_M_SIGNATURE) eq 0">
			                <xsl:value-of select="concat(substring-before($M_M_EXPLODED[1], '('), '( void )')"/>
			            </xsl:when>
			            <xsl:otherwise>
			                <xsl:value-of select="concat(substring-before($M_M_EXPLODED[1], '('), '( ', $M_M_PARMS, ' )')"/>
			            </xsl:otherwise>
			        </xsl:choose>
                </xsl:variable>
   <!--      --><xsl:value-of select="concat($CR_INDENT1, concat(string-join(($M_M_EXPLODED[3], $M_M_CALL), ' '), ';'))"/>
            </xsl:for-each>

   void Update( event theEvent, char parm1, char parm2 );
};
        </xsl:for-each>
    </xsl:template>

    <!--  ____________________________________________________________________________ Display state text (from a 'draw:custom-shape' element) -->
    <xsl:template match="draw:custom-shape" mode="STATE_NAME">
      <xsl:value-of select="normalize-space(text:p)"/>
    </xsl:template>

    <!--  ____________________________________________________________________________ Display transition text (from a 'draw:connector' element) -->
    <xsl:template match="draw:connector" mode="TRANSITION_TEXT">
        <xsl:for-each select="text:p">
            <xsl:if test="string-length(.) > 0">
                <xsl:value-of select="."/>
            </xsl:if>
        </xsl:for-each>
    </xsl:template>

    <!--  ____________________________________________________________________________ Display transition name and parameters (from a 'draw:connector' element) -->
    <xsl:template match="draw:connector" mode="TRANSITION_NAME_WITH_PARAMETERS">
        <xsl:variable name="TRANSITION_TEXT_WITH_PARAMETERS_STRING">
            <xsl:apply-templates select="." mode="TRANSITION_TEXT"/>
        </xsl:variable>
        <xsl:value-of select="substring-before(translate(normalize-space($TRANSITION_TEXT_WITH_PARAMETERS_STRING), ' ', ''),'/')"/>
    </xsl:template>

    <!--  ____________________________________________________________________________ Display transition name only (from a 'draw:connector' element) -->
    <xsl:template match="draw:connector" mode="TRANSITION_NAME">
        <xsl:variable name="TRANSITION_TEXT_STRING">
            <xsl:apply-templates select="." mode="TRANSITION_NAME_WITH_PARAMETERS"/>
        </xsl:variable>
        <xsl:choose>
            <xsl:when test="contains($TRANSITION_TEXT_STRING, '(')">
                <xsl:value-of select="substring-before($TRANSITION_TEXT_STRING, '(')"/>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="$TRANSITION_TEXT_STRING"/>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:template>

    <!--  ____________________________________________________________________________ Display transition parameters only (from a 'draw:connector' element) -->
    <xsl:template match="draw:connector" mode="TRANSITION_PARAMETERS">
        <xsl:variable name="TRANSITION_TEXT_STRING">
            <xsl:apply-templates select="." mode="TRANSITION_NAME_WITH_PARAMETERS"/>
        </xsl:variable>
        <xsl:choose>
            <xsl:when test="contains($TRANSITION_TEXT_STRING, '(')">
                <xsl:value-of select="concat('(', substring-after($TRANSITION_TEXT_STRING, '('))"/>
            </xsl:when>
            <xsl:otherwise>
                <xsl:value-of select="''"/>
            </xsl:otherwise>
        </xsl:choose>
    </xsl:template>

    <!--  ____________________________________________________________________________ Display transition function ID only (from a 'draw:connector' element) -->
    <xsl:template match="draw:connector" mode="FUNCTION_ID">
        <xsl:variable name="TRANSITION_TEXT_STRING">
            <xsl:apply-templates select="." mode="TRANSITION_TEXT"/>
        </xsl:variable>
        <xsl:value-of select="substring-before(substring-after($TRANSITION_TEXT_STRING, '['), ']')"/>
    </xsl:template>

    <!--  ____________________________________________________________________________ Constructs a sequence of the data members of the class (from a 'draw:rect' element)
                                                                                       The sequence consists of elements of the form: "initialCarLocationEventCounter : int = 0 "
          NB: this only works for one 'draw:rect' element per page containing 'Local Variables' and formatted appropriately (i.e., with a prior separator line of underscores) -->
    <xsl:template match="draw:rect" mode="DATA_MEMBERS">
        <xsl:for-each select="tokenize(tokenize(., '___+')[2], '\.\s*')">
            <xsl:if test="contains(.,' : ')">
            	<xsl:value-of select="."/>
            </xsl:if>
        </xsl:for-each>
    </xsl:template>

    <!--  ____________________________________________________________________________ Constructs a sequence of the methods of the class (from a 'draw:rect' element)
                                                                                       The sequence consists of elements of the form: "Floor_requested(dir:Direction) : bool "
          NB: this only works for one 'draw:rect' element per page containing 'Local Variables' and formatted appropriately (i.e., with two prior serarator lines of underscores) -->
    <xsl:template match="draw:rect" mode="METHODS">
        <xsl:for-each select="tokenize(tokenize(., '___+')[3], '\.\s*')">
            <xsl:if test="contains(.,' : ')">
            	<xsl:value-of select="."/>
            </xsl:if>
        </xsl:for-each>
    </xsl:template>
</xsl:stylesheet>



