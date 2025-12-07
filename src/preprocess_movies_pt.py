import pandas as pd

def main():
    # 1) Lê o CSV original (se o nome for outro, é só ajustar aqui)
    input_path = "data/full_top_rated_movies.csv"
    output_path = "data/movies_pt_clean.csv"

    print(f"Lendo arquivo: {input_path} ...")
    df = pd.read_csv(input_path)

    # 2) Filtra somente filmes cuja linguagem original é português
    #    No CSV, a coluna se chama "original_language" e o código é "pt"
    df_pt = df[df["original_language"] == "en"].copy()

    print(f"Total de filmes no dataset: {len(df)}")
    print(f"Total de filmes com linguagem original PT: {len(df_pt)}")

    # 3) Seleciona apenas as colunas que vamos usar no processamento em C/MPI
    colunas = [
        "id",
        "original_title",
        "release_date",
        "popularity",
        "vote_count",
        "vote_average",
    ]

    # Garante que todas existem no CSV
    for c in colunas:
        if c not in df_pt.columns:
            raise ValueError(f"Coluna '{c}' não encontrada no CSV!")

    df_pt = df_pt[colunas]

    # 4) Converte tipos numéricos (por segurança)
    df_pt["popularity"] = pd.to_numeric(df_pt["popularity"], errors="coerce")
    df_pt["vote_count"] = pd.to_numeric(df_pt["vote_count"], errors="coerce")
    df_pt["vote_average"] = pd.to_numeric(df_pt["vote_average"], errors="coerce")

    # Remove linhas totalmente quebradas (sem popularidade ou votos, se houver)
    df_pt = df_pt.dropna(subset=["popularity", "vote_count", "vote_average"])

    # 5) Salva o CSV limpo com ; como separador (ótimo pra ler em C)
    df_pt.to_csv(output_path, sep=";", index=False)

    print(f"Arquivo limpo salvo em: {output_path}")
    print("Pré-visualização:")
    print(df_pt.head())

if __name__ == "__main__":
    main()
