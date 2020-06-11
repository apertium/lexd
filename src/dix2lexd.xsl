<?xml version="1.0" encoding="UTF-8"?><!-- -*- nxml -*- -->
<!--
 Copyright (C) 2005-2014 Universitat d'Alacant / Universidad de Alicante

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License as
 published by the Free Software Foundation; either version 2 of the
 License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful, but
 WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, see <http://www.gnu.org/licenses/>.
-->

<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="text" encoding="UTF-8" indent="no"/>

<xsl:template match="dictionary">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="lexicons">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="patterns">
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="lexicon">
  <xsl:text>LEXICON </xsl:text>
  <xsl:value-of select="./@n"/>
  <xsl:if test="@parts != ''">
    <xsl:text>(</xsl:text>
    <xsl:value-of select="./@parts"/>
    <xsl:text>)</xsl:text>
  </xsl:if>
  <xsl:text>
</xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="pattern">
  <xsl:choose>
    <xsl:when test="@n = 'main'">
      <xsl:text>PATTERNS
</xsl:text>
    </xsl:when>
    <xsl:otherwise>
      <xsl:text>PATTERN </xsl:text>
      <xsl:value-of select="./@n"/>
      <xsl:text>
</xsl:text>
    </xsl:otherwise>
  </xsl:choose>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="pe">
  <xsl:for-each select="./*">
    <xsl:if test="not(position()=1)">
      <xsl:text> </xsl:text>
    </xsl:if>
    <xsl:apply-templates select="."/>
  </xsl:for-each>
  <xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="apat">
  <xsl:text>(</xsl:text>
  <xsl:for-each select="./*">
    <xsl:if test="not(position()=1)">
      <xsl:text> </xsl:text>
    </xsl:if>
    <xsl:apply-templates select="."/>
  </xsl:for-each>
  <xsl:text>)</xsl:text>
  <xsl:value-of select="./@mode"/>
</xsl:template>

<xsl:template match="c">
  <xsl:for-each select="./*">
    <xsl:if test="not(position()=1)">
      <xsl:text>:</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="."/>
  </xsl:for-each>
  <xsl:value-of select="./@mode"/>
</xsl:template>

<xsl:template match="par">
  <xsl:if test="@side = 'right'">
    <xsl:text>:</xsl:text>
  </xsl:if>
  <xsl:value-of select="./@n"/>
  <xsl:if test="@part != ''">
    <xsl:text>(</xsl:text>
    <xsl:value-of select="./@part"/>
    <xsl:text>)</xsl:text>
  </xsl:if>
  <!-- TODO: tags -->
  <xsl:if test="@side = 'left'">
    <xsl:text>:</xsl:text>
  </xsl:if>
  <xsl:value-of select="./@mode"/>
</xsl:template>

<xsl:template match="alex">
  <xsl:text>[</xsl:text>
  <xsl:apply-templates/>
  <xsl:text>]</xsl:text>
</xsl:template>

<xsl:template match="e">
  <xsl:if test="not(@i = 'yes')">
    <xsl:for-each select="./*">
      <xsl:if test="not(position()=1)">
        <xsl:text> </xsl:text>
      </xsl:if>
      <xsl:apply-templates select="."/>
    </xsl:for-each>
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="p">
  <xsl:for-each select="./*">
    <xsl:if test="not(position()=1)">
      <xsl:text>:</xsl:text>
    </xsl:if>
    <xsl:apply-templates select="."/>
  </xsl:for-each>
</xsl:template>

<xsl:template match="l">
  <xsl:apply-templates/>
  <!-- TODO: tags -->
</xsl:template>

<xsl:template match="r">
  <xsl:apply-templates/>
  <!-- TODO: tags -->
</xsl:template>

<xsl:template match="i">
  <xsl:apply-templates/>
  <xsl:text>:</xsl:text>
  <xsl:apply-templates/>
  <!-- TODO: tags -->
</xsl:template>

<xsl:template match="a">
  <xsl:text>~</xsl:text>
</xsl:template>

<xsl:template match="b">
  <xsl:text>\ </xsl:text>
</xsl:template>

<xsl:template match="g">
  <xsl:text>\#</xsl:text>
  <xsl:apply-templates/>
</xsl:template>

<xsl:template match="j">
  <xsl:text>+</xsl:text>
</xsl:template>

<xsl:template match="m">
  <xsl:text>&gt;</xsl:text>
</xsl:template>

<xsl:template match="s">
  <xsl:text>&lt;</xsl:text>
  <xsl:value-of select="./@n"/>
  <xsl:text>&gt;</xsl:text>
</xsl:template>

<xsl:template match="x">
  <xsl:text>{</xsl:text>
  <xsl:value-of select="./@n"/>
  <xsl:text>}</xsl:text>
</xsl:template>

</xsl:stylesheet>
