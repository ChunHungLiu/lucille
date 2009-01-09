-------------------------------------------------------------------------------
---- |
---- Module      :  RSL.AST
---- Copyright   :  (c) Syoyo Fujita
---- License     :  Modified BSD
----
---- Maintainer  :  syoyo@lucillerender.org
---- Stability   :  experimental
---- Portability :  GHC 6.8
----
---- RSLParser   :  Data structure for RSL Abstract Syntax Tree.
----
-------------------------------------------------------------------------------
module RSL.AST where

data Op
  = OpAdd
  | OpSub
  | OpMul
  | OpDiv
  | OpDot       -- .
  | OpNeg       -- !
  | OpGe        -- >=
  | OpGt        -- >
  | OpLe        -- <=
  | OpLt        -- <
  | OpEq        -- ==
  | OpNeq       -- !=
  | OpAssign    -- =
  | OpAddAssign -- +=
  | OpSubAssign -- -=
  | OpMulAssign -- *=
  | OpDivAssign -- /=
    deriving (Show, Eq)

-- | Type qualifier
data Qual
  = Output
    deriving (Show, Eq) 
  
data Type
  = TyUnknown
  | TyVoid
  | TyInt
  | TyString
  | TyFloat
  | TyVector
  | TyColor
  | TyPoint
  | TyNormal
  | TyMatrix
  | TyQualified Qual Type
    deriving (Show, Eq)

data StorageClass
  = Uniform
  | Varying
  | Vertex            -- Not in RI Spec3.2
  | FaceVarying       -- Not in RI Spec3.2
  | FaceVertex        -- Not in RI Spec3.2
    deriving (Show, Eq)

data ShaderType 
  = Surface
  | Light
  | Volume
  | Displacement
  | Imager
    deriving (Show, Eq)

data Const 
  = I Int             -- optinal?
  | F Double
  | S String
  | V [Double]        -- vector
  | M [Double]        -- matrix
    deriving (Show, Eq)

data Kind
  = KindVariable
  | KindFormalVariable
  | KindBuiltinVariable
    deriving (Show, Eq)


data Symbol
  = SymVar  String            -- name of the symbol
            Type              -- type of the symbol
            StorageClass      -- storage class of the symbol
            Kind              -- kind of the symbol

  | SymFunc String            -- name of the function
            Type              -- return type of the function
            [Type]            -- arguments of the function         
            [Type]            -- optional arguments

  | SymBuiltinFunc 
            String            -- name of the function
            Type              -- return type of the function
            [Type]            -- arguments of the function         
            [Type]            -- optional arguments

    deriving (Show, Eq)

getNameOfSym (SymVar  name _ _ _) = name
getNameOfSym (SymFunc name _ _ _) = name
getNameOfSym (SymBuiltinFunc name _ _ _) = name
getTyOfSym   (SymVar  _ ty _ _)   = ty
getTyOfSym   (SymFunc _ ty _ _)   = ty
getTyOfSym   (SymBuiltinFunc _ ty _ _)   = ty

getTyOfExpr :: Expr -> Type
getTyOfExpr expr = case expr of
  Const (Just sym) _              -> getTyOfSym sym
  TypeCast (Just sym) _ _ _       -> getTyOfSym sym
  Var   (Just sym) _              -> getTyOfSym sym
  UnaryOp (Just sym) _ _          -> getTyOfSym sym
  BinOp (Just sym) _ _ _          -> getTyOfSym sym
  Call (Just sym) _ _             -> getTyOfSym sym
  _                               -> error $ "getTyOfExpr: TODO: " ++ (show expr)

type SymbolTable
  = [(String, [Symbol])]

type Statement = [Expr]

--
-- We define 3-address form of AST.
--
data Expr 
  = Const     (Maybe Symbol)
              Const
  | TypeCast  (Maybe Symbol)
              Type                        -- toType
              String                      -- spaceType if any
              Expr                        -- fromExpr
  | Var       (Maybe Symbol)
              Symbol
  | Assign    (Maybe Symbol)
              Op                          -- operator
              Expr                        -- lhs
              Expr                        -- rhs
  | Def       Type String (Maybe Expr)
  | UnaryOp   (Maybe Symbol)
              Op                          -- Operator
              Expr                        
  | BinOp     (Maybe Symbol)
              Op                          -- Operator
              Expr                        -- Left
              Expr                        -- Right
  | Call      (Maybe Symbol)
              Symbol                      -- Function signature
              [Expr]                      -- Arguments

  | Triple    [Expr]                       -- length(expr) == 3

  -- Stmt
  | If        Expr                        -- condition
              [Expr]                      -- statement
              (Maybe [Expr])              -- else statement
  | While     Expr                        -- condition
              [Expr]                      -- statement
  | Extract  (Maybe Symbol)
              Char                        -- x, y, z, or w
              Expr                        -- Should be vector expr.
  | Nil                                   -- null expr
    deriving (Show, Eq)
  

data Func 
  = ShaderFunc ShaderType String [FormalDecl] [Expr]
  | UserFunc   Type       String
    deriving (Show, Eq)


data FormalDecl 
  = FormalDecl Type String (Maybe Expr)     -- TODO: Allow const expression
    deriving (Show, Eq)
