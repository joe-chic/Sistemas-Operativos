import spacy
import re
import json
import sys
import yake  # Keyword extraction library
from spacy.pipeline import EntityRuler

# pip install spacy json re sys yake
# python -m spacy download en_core_web_md

def crear_pipeline(nlp):
    """Adds custom rules for identifying dates, birthdates, and addresses."""
    ruler = nlp.add_pipe("entity_ruler", before="ner")
    patterns = [
        {
            "label": "DATE",
            "pattern": [{"TEXT": {"REGEX": r"\d{4}-\d{2}-\d{2}"}}]
        },
        {
            "label": "BIRTHDATE",
            "pattern": [
                {"TEXT": {"REGEX": r"\d{4}"}}, 
                {"TEXT": ","},
                {"TEXT": {"REGEX": r"\d{1,2}"}}, 
                {"TEXT": {"REGEX": r"[A-Za-z]{3}"}}, 
                {"LOWER": "in"},
                {"ENT_TYPE": "GPE"},
                {"TEXT": ","},
                {"TEXT": {"REGEX": r"[A-Z]{2}"}}
            ]
        },
        {
            "label": "ADDRESS",
            "pattern": [
                {"TEXT": {"REGEX": r"\d+"}},  
                {"OP": "+"},  
                {"TEXT": {"REGEX": r"[A-Za-z]+"}},
                {"TEXT": {"REGEX": r"(Avenue|Street|Road|Blvd|vägen|gatan)"}},  
                {"OP": "*"},  
                {"TEXT": ","},
                {"ENT_TYPE": "GPE"},
                {"TEXT": ","},
                {"TEXT": {"REGEX": r"[A-Z]{2}"}},
                {"TEXT": {"REGEX": r"\d+"}}
            ]
        }
    ]
    ruler.add_patterns(patterns)
    return nlp

def extract_keywords(text):
    kw_extractor = yake.KeywordExtractor(
        lan="en",  # Change to "sv" for Swedish or "es" for Spanish
        n=3,  # Maximum length of keyword phrase
        dedupLim=0.9,  # Deduplication threshold
        top=10  # Number of keywords to extract
    )
    keywords = kw_extractor.extract_keywords(text)
    
    # Convert to a set to remove duplicates, then back to a list
    unique_keywords = list(set(kw[0].lower() for kw in keywords))  # Convert to lowercase for better deduplication
    
    return unique_keywords

def extraer_datos(texto, nlp):
    doc = nlp(texto)

    datos = {
        "fechas": [],
        "nombres": [],
        "direcciones": [],
        "fechas_de_nacimiento": [],
        "palabras_clave": []
    }

    for ent in doc.ents:
        if ent.label_ == "DATE" and re.match(r"\d{4}-\d{2}-\d{2}", ent.text):
            datos["fechas"].append(ent.text)
        elif ent.label_ == "PERSON":
            datos["nombres"].append(ent.text)
        elif ent.label_ == "ADDRESS":
            datos["direcciones"].append(ent.text)
        elif ent.label_ == "BIRTHDATE":
            datos["fechas_de_nacimiento"].append(ent.text)

    # Extract keywords and remove duplicates while keeping order
    keywords = extract_keywords(texto)
    datos["palabras_clave"] = list(dict.fromkeys(keywords))  # Removes duplicates but keeps order

    # Remove duplicate names while keeping order
    datos["nombres"] = list(dict.fromkeys(datos["nombres"]))

    return datos

def main(input_file):
    nlp = spacy.load("en_core_web_md")  # Use "sv_core_news_sm" for Swedish
    nlp = crear_pipeline(nlp)
    
    with open(input_file, "r", encoding="utf-8") as f:
        texto = f.read()
    
    datos_extraidos = extraer_datos(texto, nlp)
    print(json.dumps(datos_extraidos, indent=4, ensure_ascii=False))

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python extract_data_ml.py <input_file>")
        sys.exit(1)
    
    main(sys.argv[1])
