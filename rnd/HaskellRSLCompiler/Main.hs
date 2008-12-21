module Main where

import System

import RSL.Parser
import RSL.PPrint
import RSL.Typer
import RSL.CodeGenLLVM

debugPrinter ast = do putStrLn $ "// [AST] = " ++ show ast ++ "\n"
                      putStrLn $ pprint 0 ast   -- 0 = initial indent level
                        
                      putStrLn $ "========= LLVM IR ===========" 
                      putStrLn $ gen 0 ast


main = do args <- getArgs
          if length args > 0 then runLex program (args !! 0) debugPrinter
                             else error "Needs input file"

          --putStrLn $ show s

          --  where (s, _) = codeGenLLVM
