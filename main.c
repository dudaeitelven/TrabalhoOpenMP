#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#pragma pack(1)

/*
Para compilar:
1 - Abrir o local do fonte
2 - Digitar para compilar: gcc -o <programa> <programa.c> -fopenmp
3 - Digitar para rodar: ./<Programa> <imagem_entrada> <imagem_saida> <mascara> <numero_threads>
*/

struct cabecalho {
	unsigned short tipo;
	unsigned int tamanho_arquivo;
	unsigned short reservado1;
	unsigned short reservado2;
	unsigned int offset;
	unsigned int tamanho_image_header;
	int largura;
	int altura;
	unsigned short planos;
	unsigned short bits_por_pixel;
	unsigned int compressao;
	unsigned int tamanho_imagem;
	int largura_resolucao;
	int altura_resolucao;
	unsigned int numero_cores;
	unsigned int cores_importantes;
};
typedef struct cabecalho CABECALHO;

struct rgb{
	unsigned char blue;
	unsigned char green;
	unsigned char red;
};
typedef struct rgb RGB;

double tempoCorrente(void){
     struct timeval tval;
     gettimeofday(&tval, NULL);
     return (tval.tv_sec + tval.tv_usec/1000000.0);
}


int main(int argc, char **argv ){
	char *entrada, *saida;
	int tamanhoMascara, nth;
	CABECALHO cabecalho;
	int iForImagem, jForImagem;
	int i2, j2;
	char aux;
	int ali, limiteI, limiteJ, iForOrdenar, jForOrdenar;
	int iTamanhoAux, posicaoMediana, lacoI, lacoJ, iTamanhoAux2;
	int range;
	int np, id;
	int nthreads;
	double ti,tf;

	if ( argc != 5){
		printf("%s <img_entrada> <img_saida> <mascara> <num_threads>\n", argv[0]);
		exit(0);
	}

	entrada = argv[1];
	saida = argv[2];
	tamanhoMascara = atoi(argv[3]);
	nthreads = atoi(argv[4]);

	if ((tamanhoMascara != 3) && (tamanhoMascara != 5) && (tamanhoMascara != 7)){
		printf("Tamanho da mascara invalido!\n");
		exit(0);
	}

	omp_set_num_threads(nthreads);

	FILE *fin = fopen(entrada, "rb");

	if ( fin == NULL ){
		printf("Erro ao abrir o arquivo %s\n", entrada);
		exit(0);
	}

	FILE *fout = fopen(saida, "wb");

	if ( fout == NULL ){
		printf("Erro ao abrir o arquivo %s\n", saida);
		exit(0);
	}

	fread(&cabecalho, sizeof(CABECALHO), 1, fin);

	printf("Tamanho da imagem: %u\n", cabecalho.tamanho_arquivo);
	printf("Largura: %d\n", cabecalho.largura);
	printf("Largura: %d\n", cabecalho.altura);
	printf("Bits por pixel: %d\n", cabecalho.bits_por_pixel);

	fwrite(&cabecalho, sizeof(CABECALHO), 1, fout);

	


	//Alocar imagem
	RGB rgbAux[tamanhoMascara*tamanhoMascara];
	RGB rgbAux2;
	RGB **imagem  = (RGB **)malloc(cabecalho.altura*sizeof(RGB *));
	RGB **imagemSaida  = (RGB **)malloc(cabecalho.altura*sizeof(RGB *));
	
	for(iForImagem=0; iForImagem<cabecalho.altura; iForImagem++){
		imagem[iForImagem] = (RGB *)malloc(cabecalho.largura*sizeof(RGB));
		imagemSaida[iForImagem] = (RGB *)malloc(cabecalho.largura*sizeof(RGB));
	}

	

	ti = tempoCorrente();

	//Leitura da imagem
	#pragma omp_parallel for reduction(+:imagem,aux)
	{
		for(iForImagem=0; iForImagem<cabecalho.altura; iForImagem++){
			ali = (cabecalho.largura * 3) % 4;
			
			if (ali != 0){
				ali = 4 - ali;
			}

			for(jForImagem=0; jForImagem<cabecalho.largura; jForImagem++){
				fread(&imagem[iForImagem][jForImagem], sizeof(RGB), 1, fin);
			}

			for(jForImagem=0; jForImagem<ali; jForImagem++){
				fread(&aux, sizeof(unsigned char), 1, fin);
			}
		}
	}
	//Processar imagem
	for(iForImagem=0; iForImagem<cabecalho.altura; iForImagem++){
		for(jForImagem=0; jForImagem<cabecalho.largura; jForImagem++){
			if (tamanhoMascara == 3) {
				range = 1;
				posicaoMediana = 4;
			}
			else if (tamanhoMascara == 5) {
				range = 2;
				posicaoMediana = 12;
			}
			else if (tamanhoMascara == 7) {
				range = 3;
				posicaoMediana = 24;
			}

			//Calcular range do laco for para mediana
			lacoI = iForImagem-range;
			limiteI = iForImagem + range;

			lacoJ = jForImagem-range;
			limiteJ = jForImagem + range;

            if (lacoI < 0) lacoI = 0;
			if (lacoJ < 0) lacoJ = 0;

            if (limiteI > (cabecalho.altura - 1))  limiteI = (cabecalho.altura - 1);
			if (limiteJ > (cabecalho.largura - 1)) limiteJ = (cabecalho.largura - 1);

			//Limpar variaveis auxiliares
            for(iTamanhoAux2=0; iTamanhoAux2<tamanhoMascara*tamanhoMascara; iTamanhoAux2++){
                rgbAux[iTamanhoAux2].red   = 0;
                rgbAux[iTamanhoAux2].green = 0;
                rgbAux[iTamanhoAux2].blue  = 0;
			}

			rgbAux2.red   = 0;
            rgbAux2.green = 0;
            rgbAux2.blue  = 0;

            iTamanhoAux  = 0;
            iTamanhoAux2 = 0;

			//Calcular a mediana de cada pixel da imagem.
			//#pragma omp parallel for
			for(i2=lacoI; i2<=limiteI; i2++){
				for(j2=lacoJ; j2<=limiteJ; j2++){
					rgbAux[iTamanhoAux].red   = imagem[i2][j2].red;
					rgbAux[iTamanhoAux].green = imagem[i2][j2].green;
					rgbAux[iTamanhoAux].blue  = imagem[i2][j2].blue;

					iTamanhoAux++;
				}
			}


			//Ordenar vetores red
			#pragma omp parallel private (id, iForOrdenar, jForOrdenar)
			{
				id = omp_get_thread_num();
				for (iForOrdenar = id; iForOrdenar < iTamanhoAux; iForOrdenar += nthreads)
				{
					for (jForOrdenar = id; jForOrdenar < iTamanhoAux; jForOrdenar += nthreads)
					{
						if (rgbAux[iForOrdenar].red < rgbAux[jForOrdenar].red)
						{
							rgbAux2.red             = rgbAux[iForOrdenar].red;
							rgbAux[iForOrdenar].red = rgbAux[jForOrdenar].red;
							rgbAux[jForOrdenar].red = rgbAux2.red;
						}
					}
				}
			}

            //Ordenar vetores green
			#pragma omp parallel private (id, iForOrdenar, jForOrdenar)
			{
				id = omp_get_thread_num();
				for (iForOrdenar = id; iForOrdenar < iTamanhoAux; iForOrdenar += nthreads)
				{
					for (jForOrdenar = id; jForOrdenar < iTamanhoAux; jForOrdenar += nthreads)
					{
						if (rgbAux[iForOrdenar].green < rgbAux[jForOrdenar].green)
						{
							rgbAux2.green             = rgbAux[iForOrdenar].green;
							rgbAux[iForOrdenar].green = rgbAux[jForOrdenar].green;
							rgbAux[jForOrdenar].green = rgbAux2.green;
						}
					}
				}
			}
            
			//Ordenar vetores blue
			#pragma omp parallel private (id, iForOrdenar, jForOrdenar)
			{
				id = omp_get_thread_num();
				for (iForOrdenar = id; iForOrdenar < iTamanhoAux; iForOrdenar += nthreads)
				{
					for (jForOrdenar = id; jForOrdenar < iTamanhoAux; jForOrdenar += nthreads)
					{
						if (rgbAux[iForOrdenar].blue < rgbAux[jForOrdenar].blue)
						{
							rgbAux2.blue             = rgbAux[iForOrdenar].blue;
							rgbAux[iForOrdenar].blue = rgbAux[jForOrdenar].blue;
							rgbAux[jForOrdenar].blue = rgbAux2.blue;
						}
					}
				}
			}

            //Substituir valores pela mediana de cada pixel
			imagemSaida[iForImagem][jForImagem].red    = rgbAux[posicaoMediana].red;
			imagemSaida[iForImagem][jForImagem].green  = rgbAux[posicaoMediana].green;
			imagemSaida[iForImagem][jForImagem].blue   = rgbAux[posicaoMediana].blue;
		}
	}

	//Escrever a imagem
	#pragma omp_parallel for reduction(+:imagemSaida,aux)
	{
		for(iForImagem=0; iForImagem<cabecalho.altura; iForImagem++){
			ali = (cabecalho.largura * 3) % 4;

			if (ali != 0){
				ali = 4 - ali;
			}
			
			for(jForImagem=0; jForImagem<cabecalho.largura; jForImagem++){
				fwrite(&imagemSaida[iForImagem][jForImagem], sizeof(RGB), 1, fout);
			}

			for(jForImagem=0; jForImagem<ali; jForImagem++){
				fwrite(&aux, sizeof(unsigned char), 1, fout);
			}
		}
	}
	tf = tempoCorrente();
	printf("Tempo = %f\n", tf - ti );

	fclose(fin);
	fclose(fout);

}
