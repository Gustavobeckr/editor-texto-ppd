# Editor de texto -- PPD

**Universidade:** Universidade de Santa Cruz do Sul  
**Disciplina:** Programação Paralela e Distribuída 
**Professor:** Ivan Luis Suptitz  

Este projeto foi desenvolvido como trabalho da disciplina PPD, que implementa um **editor de texto colaborativo** em C, integrando **MPI** e **OpenMP** para suportar:

- Edição simultânea por múltiplos “usuários” (processos MPI)  
- Bloqueio de linhas para garantir consistência  
- Chat ponto-a-ponto entre dois usuários  
- Logs de todas as operações  
- Geração massiva de dados de teste em paralelo (OpenMP)  

## Bibliotecas

- **Linguagem:** C (GTK3 para GUI)  
- **OpenMP:** paralelização local (geração de dados de teste, medição de tempo)  
- **MPI:** comunicação ponto-a-ponto (locks, chat) e coletiva (`MPI_Bcast`, `MPI_Barrier`)  
- **GTK+ 3:** interface gráfica (Glade)  
- **pthread:** thread de recepção de chat 

## Funcionalidades

1. **Multi-usuário (MPI)**  
2. **Consistência (locks de linha)**  
3. **Chat entre usuários**  
4. **Geração de dados de teste em paralelo (OpenMP)**  
5. **Logs de edição na GUI**  

---

## Pré-requisitos

### Em Ubuntu/Debian

```bash
sudo apt-get update
sudo apt-get install build-essential libgtk-3-dev pkg-config glade
```

### Build & Run

```bash
make clean    # Instalar dependências
make            # Compilar
make run        # Executar
```

### Modo gerar e medir

```bash
mpirun -np 4 ./edito_texto_ppd 1000
```
