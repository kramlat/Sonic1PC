<?xml version="1.0" encoding="UTF-8"?>
<!-- Turns the handbook's DocBook (index.docbook, the KDE Help Center source) into the single HTML page the game shows in its
     own Help window when KDE's Help Center is not installed. It handles the elements the handbook uses, and no others. -->
<xsl:stylesheet version="1.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="UTF-8" indent="no"/>

<xsl:template match="/book">
<html>
<head>
<meta charset="UTF-8"/>
<title><xsl:value-of select="bookinfo/title"/></title>
<style>
body { font-family: sans-serif; margin: 1.5em 2em; max-width: 52em; }
h1 { border-bottom: 2px solid #3366cc; padding-bottom: 0.2em; }
h2 { margin-top: 1.6em; border-bottom: 1px solid #aaa; }
h3 { margin-top: 1.2em; }
img { max-width: 100%; border: 1px solid #888; }
img.sprite { border: none; image-rendering: pixelated; }
.screenshot { margin: 1em 0; text-align: center; }
.caption { font-size: 90%; color: #555; }
table { border-collapse: collapse; margin: 0.8em 0; }
th, td { border: 1px solid #aaa; padding: 0.25em 0.6em; vertical-align: top; text-align: left; }
th { background: #e8eef8; }
.note, .tip { border-left: 4px solid #3366cc; background: #eef3fb; padding: 0.3em 0.8em; margin: 0.8em 0; }
.tip { border-left-color: #2e8b57; background: #eef8f2; }
kbd, .guibutton, .guilabel { border: 1px solid #999; border-radius: 3px; padding: 0 0.3em; background: #f4f4f4; font-size: 95%; }
code, .command, .option { font-family: monospace; }
.gui { font-weight: bold; }
dt { font-weight: bold; margin-top: 0.6em; }
.toc li { margin: 0.1em 0; }
</style>
</head>
<body>
<h1><xsl:value-of select="bookinfo/title"/></h1>
<xsl:apply-templates select="bookinfo/abstract/para"/>
<p class="caption"><xsl:value-of select="bookinfo/authorgroup/author/firstname"/><xsl:text> </xsl:text>
<xsl:value-of select="bookinfo/authorgroup/author/surname"/> - <xsl:value-of select="bookinfo/date"/></p>
<h2>Contents</h2>
<ul class="toc">
<xsl:for-each select="chapter">
<li><a href="#{@id}"><xsl:value-of select="title"/></a>
<xsl:if test="sect1">
<ul>
<xsl:for-each select="sect1"><li><a href="#{@id}"><xsl:value-of select="title"/></a></li></xsl:for-each>
</ul>
</xsl:if>
</li>
</xsl:for-each>
</ul>
<xsl:apply-templates select="chapter"/>
<hr/>
<p class="caption"><xsl:apply-templates select="bookinfo/legalnotice/para"/></p>
</body>
</html>
</xsl:template>

<xsl:template match="chapter"><h2 id="{@id}"><xsl:value-of select="title"/></h2><xsl:apply-templates select="*[not(self::title)]"/></xsl:template>
<xsl:template match="sect1"><h3 id="{@id}"><xsl:value-of select="title"/></h3><xsl:apply-templates select="*[not(self::title)]"/></xsl:template>
<xsl:template match="sect2"><h4 id="{@id}"><xsl:value-of select="title"/></h4><xsl:apply-templates select="*[not(self::title)]"/></xsl:template>

<xsl:template match="para"><p><xsl:apply-templates/></p></xsl:template>
<xsl:template match="itemizedlist"><ul><xsl:apply-templates/></ul></xsl:template>
<xsl:template match="itemizedlist/listitem"><li><xsl:apply-templates/></li></xsl:template>
<xsl:template match="orderedlist"><ol><xsl:apply-templates/></ol></xsl:template>
<xsl:template match="orderedlist/listitem"><li><xsl:apply-templates/></li></xsl:template>
<xsl:template match="variablelist"><dl><xsl:apply-templates/></dl></xsl:template>
<xsl:template match="varlistentry"><dt><xsl:apply-templates select="term"/></dt><dd><xsl:apply-templates select="listitem/*"/></dd></xsl:template>
<xsl:template match="term"><xsl:apply-templates/></xsl:template>

<xsl:template match="note"><div class="note"><b>Note: </b><xsl:apply-templates/></div></xsl:template>
<xsl:template match="tip"><div class="tip"><b>Tip: </b><xsl:apply-templates/></div></xsl:template>

<xsl:template match="screenshot">
<div class="screenshot">
<img src="{mediaobject/imageobject/imagedata/@fileref}" alt="{mediaobject/textobject/phrase}"/>
<div class="caption"><xsl:value-of select="screeninfo"/></div>
</div>
</xsl:template>

<xsl:template match="inlinemediaobject"><img class="sprite" src="{imageobject/imagedata/@fileref}" alt="{textobject/phrase}"/></xsl:template>

<xsl:template match="informaltable">
<table><xsl:apply-templates select="tgroup/thead|tgroup/tbody"/></table>
</xsl:template>
<xsl:template match="thead/row"><tr><xsl:for-each select="entry"><th><xsl:apply-templates/></th></xsl:for-each></tr></xsl:template>
<xsl:template match="tbody/row"><tr><xsl:for-each select="entry"><td><xsl:apply-templates/></td></xsl:for-each></tr></xsl:template>

<xsl:template match="qandaset"><xsl:apply-templates/></xsl:template>
<xsl:template match="qandaentry"><xsl:apply-templates select="question"/><xsl:apply-templates select="answer"/></xsl:template>
<xsl:template match="question"><p><b>Q: <xsl:value-of select="para"/></b></p></xsl:template>
<xsl:template match="answer"><xsl:apply-templates/></xsl:template>

<xsl:template match="keycap"><kbd><xsl:apply-templates/></kbd></xsl:template>
<xsl:template match="guibutton"><span class="guibutton"><xsl:apply-templates/></span></xsl:template>
<xsl:template match="guilabel"><span class="guilabel"><xsl:apply-templates/></span></xsl:template>
<xsl:template match="guimenu|guisubmenu|guimenuitem"><span class="gui"><xsl:apply-templates/></span></xsl:template>
<xsl:template match="menuchoice">
<xsl:for-each select="*"><xsl:apply-templates select="."/><xsl:if test="position() != last()"> &#8594; </xsl:if></xsl:for-each>
</xsl:template>
<xsl:template match="command"><span class="command"><xsl:apply-templates/></span></xsl:template>
<xsl:template match="option"><span class="option"><xsl:apply-templates/></span></xsl:template>
<xsl:template match="replaceable"><i><xsl:apply-templates/></i></xsl:template>
<xsl:template match="filename"><code><xsl:apply-templates/></code></xsl:template>
<xsl:template match="emphasis"><em><xsl:apply-templates/></em></xsl:template>
<xsl:template match="xref"><a href="#{@linkend}"><xsl:value-of select="id(@linkend)/title"/></a></xsl:template>
</xsl:stylesheet>
