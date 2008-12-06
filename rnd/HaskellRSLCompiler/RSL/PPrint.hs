-------------------------------------------------------------------------------
---- |
---- Module      :  RSL.PPrint
---- Copyright   :  (c) Syoyo Fujita
---- License     :  BSD-style
----
---- Maintainer  :  syoyo@lucillerender.org
---- Stability   :  experimental
---- Portability :  GHC 6.8
----
---- RSLParser   :  Pretty printer for RSL AST.
----
-------------------------------------------------------------------------------

module RSL.PPrint where

import RSL.AST


indent :: Int -> String
indent n = concat $ replicate (4 * n) " "


class AST a where

  pprint :: Int -> a -> String
  pprintList :: Int -> [a] -> String
  pprintList n = concat . map (pprint n)


instance AST a => AST [a] where

  pprint = pprintList

instance AST ShaderType where

  pprint n ty = case ty of

    Surface       -> "surface"
    Volume        -> "volume"
    Displacement  -> "displacement"
    Imager        -> "imager"


instance AST Type where

  pprint n ty = case ty of

    TyUnknown     -> "uknown"
    TyVoid        -> "void"
    TyFloat       -> "float"
    TyVector      -> "vector"
    TyColor       -> "color"
    TyPoint       -> "point"
    TyNormal      -> "normal"
    TyMatrix      -> "matrix"

emitOp op = case op of
  OpAdd -> "+"
  OpSub -> "-"
  OpMul -> "*"
  OpDiv -> "/"

instance AST Expr where
  
  pprint n expr = case expr of

    Const  const                -> "const"


    Var    (Symbol name _ _ _)  -> name

    BinOp  ty op exprs          -> concat
      [ "( "
      , pprint 0 (exprs !! 0) -- left
      , " " ++ emitOp op ++ " "
      , pprint 0 (exprs !! 1) -- right
      , " )"
      ]


    Def    ty val Nothing -> concat 
      [ indent n
      , pprint 0 ty ++ " " ++ val ++ ";\n"
      ]


    Def    ty val (Just initExpr)  -> concat 
      [ indent n
      , pprint 0 ty ++ " " ++ val
      , " = "
      , pprint 0 initExpr
      , ";\n"
      ]


    Assign ty lexpr rexpr -> concat 
      [ indent n
      , pprint 0 lexpr
      , " = "
      , pprint 0 rexpr
      , ";\n"
      ]


    Call retTy name args  -> concat 
      [ name
      , "("
      , pprintArgs args
      , ")"
      ]

      where

        pprintArgs []     = ""
        pprintArgs [x]    = pprint 0 x
        pprintArgs (x:xs) = pprint 0 x ++ ", " ++ pprintArgs xs


instance AST FormalDecl where

  pprint n decl = case decl of

    FormalDecl ty name Nothing    -> pprint n ty ++ " " ++ name
    FormalDecl ty name (Just val) -> pprint n ty ++ " " ++ name


  pprintList n []     = ""
  pprintList n [x]    = pprint n x
  pprintList n (x:xs) = pprint n x ++ ", " ++ pprint n xs

instance AST Func where

  pprint n f = case f of

    ShaderFunc ty name decls stms -> concat 
      [ pprint n ty
      , " " ++ name
      , "("
      , pprint n decls
      , ") {\n"
      , pprint (n+1) stms
      , "\n}\n"
      ]


