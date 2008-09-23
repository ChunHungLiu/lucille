.. _xref_setup:

====================
lucille インストール
====================

ここでは、lucille をソースパッケージからインストールする手順を解説します.

依存ライブラリ
==============

lucille をビルドするには、以下の開発環境および依存ライブラリをあらかじめインストールしておく必要があります.

開発環境(必須)
--------------

* gcc 4.0 以上
* flex/bison
* scons 1.0 以上(ビルドツール)

scons は Python で記述されたビルドツールです. lucille はビルドに scons を利用しています. scons はバージョン 1.0 以上を利用してください. また、scons は ``--standard-lib`` を指定して、管理者権限でインストールしておいてください.

依存ライブラリ(オプション)
--------------------------

* git (ソースコードマネジメントツール)
* fltk 1.1.9 (GUI ライブラリ) http://www.fltk.org/
* libjpeg (画像ライブラリ)
* libzlib (圧縮ライブラリ)

ビューアなど GUI を使用する場合に fltk を利用しています. fltk はバージョン 1.1.9 を使い、また --enable-threads オプションを configure に指定してコンパイルしてインストールしておいてください.

libjpeg があると、jpeg 画像をテクスチャとして読み込んだり、レンダリング画像を jpeg で出力することができるようになります. 

libzlib があると、圧縮ファイルや圧縮テクスチャを扱うことができます.


ソースコード取得
================

まず lucille のソースコードを入手していない場合は github から最新版を clone して入手します.::

  $ git clone git://github.com/syoyo/lucille.git

もしくは、http://github.com/syoyo/lucille/tree/master から download ボタンで最新スナップショットを取得することができます.


コンフィグ
==========

ソースディレクトリのトップにある ```custum.py``` を編集します.

* build_target : ビルドターゲットを指定します. ``debug``, ``release``, ``speed`` から選択します.
* enable_sse : SSE 命令を使うかどうか指定します.
* use_double : double 精度で演算を行うかどうか指定します.
* enable_64bit : 64bit バイナリを生成するかどうか指定します.
* with_zlib : 圧縮ファイルをサポートします. 有効にした場合は、ZLIB_LIC_PATH, ZLIB_LIBPATH, ZLIB_LIB_NAME で zlib のライブラリパスなどを指定できます.
* with_jpeglib : jpeg ファイルをサポートします. 有効にした場合は、JPEGLIB_LIC_PATH, JPEGLIB_LIBPATH, JPEGLIB_LIB_NAME で libjpeg のライブラリパスなどを指定できます.


ビルド
======

```custom.py``` の編集が終了したら、scons を利用してビルドを行います. ::

  $ scons

ビルドが成功すると、レンダラコマンド ``lsh`` が ``bin`` ディレクトリに、各種ライブラリ群が ``lib`` ディレクトリに生成されます.

システムへインストールするときは, ::

  $ sudo scons --prefix=/PATH/TO/PREFIX

としてください. このコマンドの実行には管理者権限が必要になります. バイナリが ``/PATH/TO/PREFIX/bin`` に、ライブラリが ``/PATH/TO/PREFIX/lib`` にインストールされます.
(scons を --standard-libs 付きでインストールしていない場合、管理者権限での実行時に scons でエラーがでることがあります)

